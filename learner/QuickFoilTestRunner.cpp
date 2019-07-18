/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "QuickFoilTestRunner.hpp"

#include <iostream>
#include <memory>

#include "expressions/AttributeReference.hpp"
#include "operations/BuildHashTable.hpp"
#include "operations/MultiColumnHashJoin.hpp"
#include "operations/SemiJoin.hpp"
#include "operations/SemiJoinFactory.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilPredicate.hpp"
#include "schema/FoilVariable.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/ElementDeleter.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

QuickFoilTestRunner::QuickFoilTestRunner(
    const FoilPredicate* target_predicate,
    const Vector<std::unique_ptr<const FoilClause>>& clauses)
    : target_predicate_(target_predicate),
      clauses_(clauses) {
}

QuickFoilTestRunner::~QuickFoilTestRunner() {
}

TableView* QuickFoilTestRunner::ComputeUnCoveredData(
    const TableView& current_uncovered_data,
    const TableView& current_binding_table,
    const FoilLiteral& literal) const {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  Vector<AttributeReference> binding_join_keys;
  Vector<AttributeReference> literal_join_keys;
  Vector<int> unbounded_vids;

  for (int i = 0; i < literal.num_variables(); ++i) {
    if (literal.variable_at(i).variable_id() < current_binding_table.num_columns()) {
      literal_join_keys.emplace_back(i);
      binding_join_keys.emplace_back(literal.variable_at(i).variable_id());
    } else {
      unbounded_vids.emplace_back(i);
    }
  }

  Vector<int> project_column_ids;
  Vector<AttributeReference> coverage_join_keys;
  for (int i = 0; i < target_predicate_->num_arguments(); ++i) {
    project_column_ids.emplace_back(i);
    coverage_join_keys.emplace_back(i);
  }

  const TableView& background_table = literal.predicate()->fact_table();
  std::unique_ptr<FoilHashTable> background_hash_table;
  std::unique_ptr<FoilHashTable> hash_table_for_bindings;
  std::unique_ptr<SemiJoin> binding_semijoin(
      SelectAndCreateSemiJoin(current_binding_table,
                              background_table,
                              &hash_table_for_bindings,
                              &background_hash_table,
                              binding_join_keys,
                              literal_join_keys,
                              project_column_ids));

  std::unique_ptr<FoilHashTable> positive_hash_table_for_coverage(
      BuildHashTableAfterSemiJoin(current_binding_table.num_tuples(),
                                  target_predicate_->num_arguments(),
                                  binding_semijoin.release()));

  std::unique_ptr<SemiJoin> coverage_semijoin(
      CreateSemiJoin(true,
                     current_uncovered_data,
                     current_binding_table,
                     *positive_hash_table_for_coverage,
                     coverage_join_keys,
                     coverage_join_keys,
                     project_column_ids));

  Vector<BufferPtr> output_buffers;
  for (int i = 0; i < target_predicate_->num_arguments(); ++i) {
    output_buffers.emplace_back(
        std::make_shared<Buffer>(sizeof(cpp_type) * current_uncovered_data.num_tuples(),
                                 current_uncovered_data.num_tuples()));
  }

  size_type num_output_tuples = 0;
  std::unique_ptr<SemiJoinChunk> coverage_result(coverage_semijoin->Next());
  while (coverage_result != nullptr) {
    coverage_result->semi_bitvector.flip();
    coverage_result->num_ones =
        coverage_result->semi_bitvector.size() - coverage_result->num_ones;
    if (coverage_result->num_ones > 0) {
      for (int i = 0; i < target_predicate_->num_arguments(); ++i) {
        coverage_join_keys[i].EvaluateWithFilter(coverage_result->output_columns,
                                                 coverage_result->semi_bitvector,
                                                 coverage_result->num_ones,
                                                 num_output_tuples,
                                                 output_buffers[i].get());
      }
      num_output_tuples += coverage_result->num_ones;
    }
    coverage_result.reset(coverage_semijoin->Next());
  }

  Vector<ConstBufferPtr> output_const_buffers;
  const std::size_t actual_buffer_size = num_output_tuples * sizeof(cpp_type);
  for (BufferPtr& output_buffer : output_buffers) {
    output_buffer->Realloc(actual_buffer_size, num_output_tuples);
    output_const_buffers.emplace_back(std::make_shared<const ConstBuffer>(output_buffer));
  }

  return new TableView(std::move(output_const_buffers));
}

size_type QuickFoilTestRunner::RunTest(
    const TableView& test_data) const {
  std::unique_ptr<TableView> uncovered_data(test_data.Clone());
  for (const std::unique_ptr<const FoilClause>& clause : clauses_) {
    CHECK_GT(clause->num_body_literals(), 0);

    std::unique_ptr<TableView> binding_set(test_data.Clone());
    for (int i = 0; i < clause->num_body_literals() - 1; ++i) {
      Vector<ConstBufferPtr> binding_columns;
      CreateBindingTable(clause->body_literals()[i], *binding_set, &binding_columns);
      binding_set.reset(new TableView(std::move(binding_columns)));
      if (binding_set->empty()) break;
    }

    if (binding_set->empty()) {
      std::cout << clause->ToString() << " does not cover any test data\n";
      continue;
    }

    uncovered_data.reset(ComputeUnCoveredData(*uncovered_data,
                                              *binding_set,
                                              clause->body_literals().back()));

    if (uncovered_data == nullptr) {
      std::cout << clause->ToString() << ": #uncovered=0\n";
      return 0;
    }

    std::cout
       << clause->ToString() << ": #uncovered=" << uncovered_data->num_tuples() << "\n";
  }

  return uncovered_data->num_tuples();
}

} /* namespace quickfoil */
