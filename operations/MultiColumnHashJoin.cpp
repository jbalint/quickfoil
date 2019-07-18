/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/MultiColumnHashJoin.hpp"

#include "expressions/AttributeReference.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "memory/Buffer.hpp"
#include "operations/BuildHashTable.hpp"
#include "storage/FoilHashTable.hpp"
#include "storage/TableView.hpp"
#include "utility/Hash.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace quickfoil {

DEFINE_int32(join_chunck_size,
             32768,
             "The number of tuples of a chunk in the MultiColumnHashJoin.");

void CreateBindingTable(const FoilLiteral& new_literal,
                        const TableView& cur_binding_table,
                        Vector<ConstBufferPtr>* new_binding_table) {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  START_TIMER(QuickFoilTimer::kCreateBindingTable);

  Vector<AttributeReference> binding_join_keys;
  Vector<AttributeReference> literal_join_keys;
  Vector<int> unbounded_vids;

  for (int i = 0; i < new_literal.num_variables(); ++i) {
    if (new_literal.variable_at(i).variable_id() < cur_binding_table.num_columns()) {
      literal_join_keys.emplace_back(i);
      binding_join_keys.emplace_back(new_literal.variable_at(i).variable_id());
    } else {
      unbounded_vids.emplace_back(i);
    }
  }

  const int num_output_columns = unbounded_vids.size() + cur_binding_table.num_columns();
  Vector<BufferPtr> output_buffers;
  const std::size_t inital_buffer_size = sizeof(cpp_type) * cur_binding_table.num_tuples();
  for (int i = 0; i < num_output_columns; ++i) {
    output_buffers.emplace_back(
        std::make_shared<Buffer>(inital_buffer_size, cur_binding_table.num_tuples()));
  }

  const TableView& literal_table = new_literal.predicate()->fact_table();
  if (literal_table.num_tuples() < cur_binding_table.num_tuples()) {
    std::unique_ptr<FoilHashTable> hash_table(
        BuildHashTableOnTable(literal_join_keys, literal_table));

    Vector<AttributeReference> project_expressions;
    for (int i = 0; i < cur_binding_table.num_columns(); ++i) {
      project_expressions.emplace_back(i);
    }
    for (int unbounded_vid : unbounded_vids) {
      project_expressions.emplace_back(unbounded_vid + cur_binding_table.num_columns());
    }

    MultiColumnHashJoin hash_join(cur_binding_table,
                                  binding_join_keys,
                                  project_expressions);
    if (unbounded_vids.empty()) {
      hash_join.Join<true, true, false>(literal_table,
                                        *hash_table,
                                        literal_join_keys,
                                        &output_buffers);
    } else {
      hash_join.Join<true, true, true>(literal_table,
                                       *hash_table,
                                       literal_join_keys,
                                       &output_buffers);
    }
  } else {
    std::unique_ptr<FoilHashTable> hash_table(
        BuildHashTableOnTable(binding_join_keys, cur_binding_table));

    Vector<AttributeReference> project_expressions;
    for (int i = 0; i < cur_binding_table.num_columns(); ++i) {
      project_expressions.emplace_back(i + literal_table.num_columns());
    }
    for (int unbounded_vid : unbounded_vids) {
      project_expressions.emplace_back(unbounded_vid);
    }

    MultiColumnHashJoin hash_join(literal_table,
                                  literal_join_keys,
                                  project_expressions);
    if (unbounded_vids.empty()) {
      hash_join.Join<true, false, true>(cur_binding_table,
                                        *hash_table,
                                        binding_join_keys,
                                        &output_buffers);
    } else {
      hash_join.Join<true, true, true>(cur_binding_table,
                                       *hash_table,
                                       binding_join_keys,
                                       &output_buffers);
    }
  }

  for (const BufferPtr& output_buffer : output_buffers) {
    new_binding_table->emplace_back(
        std::make_shared<const ConstBuffer>(output_buffer));
  }

  STOP_TIMER(QuickFoilTimer::kCreateBindingTable);
}

void CreateLabelAwareBindingTables(const FoilClauseConstSharedPtr& clause,
                                   const FoilLiteral& new_literal,
                                   const size_type num_binding_positives,
                                   const size_type num_binding_negatives,
                                   Vector<ConstBufferPtr>* new_binding_table) {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  const size_type positive_binding_size = clause->GetNumPositiveBindings();
  const size_type negative_binding_size = clause->GetNumNegativeBindings();
  const size_type background_table_size = new_literal.predicate()->GetNumTotalFacts();

  Vector<AttributeReference> clause_keys;
  Vector<AttributeReference> background_keys;
  Vector<int> unbounded_vids;

  const int num_background_columns = new_literal.num_variables();
  const Vector<FoilVariable>& variables = new_literal.variables();
  for (int i = 0; i < num_background_columns; ++i) {
    if (variables[i].IsBound()) {
      background_keys.emplace_back(i);
      clause_keys.emplace_back(variables[i].variable_id());
    } else {
      unbounded_vids.emplace_back(i);
    }
  }

  const int num_clause_columns = clause->num_variables();
  Vector<BufferPtr> output_buffers;
  const size_type num_binding_tuples = num_binding_positives + num_binding_negatives;
  const std::size_t output_buffer_bytes = sizeof(cpp_type) * num_binding_tuples;
  for (int i = 0; i < num_clause_columns + static_cast<int>(unbounded_vids.size()); ++i) {
    output_buffers.emplace_back(std::move(std::make_shared<Buffer>(
        output_buffer_bytes,
        num_binding_tuples)));
  }

  Vector<BufferPtr> output_negative_buffers;
  for (const BufferPtr& output_buffer : output_buffers) {
    output_negative_buffers.emplace_back(
        std::move(std::make_shared<Buffer>(output_buffer,
                                           output_buffer->mutable_as_type<cpp_type>() + num_binding_positives,
                                           num_binding_negatives)));
  }

  std::unique_ptr<TableView> positive_table;
  std::unique_ptr<TableView> negative_table;
  if (clause->IsBindingDataConseuctive()) {
    positive_table.reset(new TableView(std::move(clause->CreatePositiveBlocks())));
    negative_table.reset(new TableView(std::move(clause->CreateNegativeBlocks())));
  } else {
    positive_table.reset(new TableView(clause->positive_blocks()));
    negative_table.reset(new TableView(clause->negative_blocks()));
  }

  const TableView& background_table = new_literal.predicate()->fact_table();
  if (positive_binding_size < background_table_size &&
      negative_binding_size < background_table_size) {
    // The background table is the probe table.
    std::unique_ptr<FoilHashTable> positive_hash_table(
        BuildHashTableOnTable(clause_keys, *positive_table));
    std::unique_ptr<FoilHashTable> negative_hash_table(
        BuildHashTableOnTable(clause_keys, *negative_table));

    Vector<AttributeReference> project_expressions;
    for (int i = 0; i < clause->num_variables(); ++i) {
      project_expressions.emplace_back(i + num_background_columns);
    }
    for (int unbounded_vid : unbounded_vids) {
      project_expressions.emplace_back(unbounded_vid);
    }

    MultiColumnHashJoin hash_join(background_table,
                                  background_keys,
                                  std::move(project_expressions));
    if (unbounded_vids.empty()) {
      hash_join.CollaborateJoin<false, true>(*positive_table,
                                             *negative_table,
                                             *positive_hash_table,
                                             *negative_hash_table,
                                             clause_keys,
                                             &output_buffers,
                                             &output_negative_buffers);
    } else {
      hash_join.CollaborateJoin<true, true>(*positive_table,
                                            *negative_table,
                                            *positive_hash_table,
                                            *negative_hash_table,
                                            clause_keys,
                                            &output_buffers,
                                            &output_negative_buffers);
    }
  } else {
    std::unique_ptr<FoilHashTable> hash_table(
        BuildHashTableOnTable(background_keys, background_table));
    Vector<AttributeReference> project_expressions;
    for (int i = 0; i < num_clause_columns; ++i) {
      project_expressions.emplace_back(i);
    }
    for (int unbounded_vid : unbounded_vids) {
      project_expressions.emplace_back(unbounded_vid + num_clause_columns);
    }

    {
      MultiColumnHashJoin hash_join(*positive_table,
                                    clause_keys,
                                    project_expressions);
      if (unbounded_vids.empty()) {
        hash_join.Join<false, true, false>(background_table,
                                           *hash_table,
                                           background_keys,
                                           &output_buffers);
      } else {
        hash_join.Join<false, true, true>(background_table,
                                          *hash_table,
                                          background_keys,
                                          &output_buffers);
      }
    }

    MultiColumnHashJoin hash_join(*negative_table,
                                  clause_keys,
                                  std::move(project_expressions));
    if (unbounded_vids.empty()) {
      hash_join.Join<false, true, false>(background_table,
                                         *hash_table,
                                         background_keys,
                                         &output_negative_buffers);
    } else {
      hash_join.Join<false, true, true>(background_table,
                                        *hash_table,
                                        background_keys,
                                        &output_negative_buffers);
    }
  }

  for (const BufferPtr& output_buffer : output_buffers) {
    new_binding_table->emplace_back(std::move(std::make_shared<const ConstBuffer>(
        output_buffer)));
  }
}

template <bool resizeable, bool populate_probe_tids, bool populate_build_tids>
void MultiColumnHashJoin::Join(
    const TableView& build_table,
    const FoilHashTable& hash_table,
    const Vector<AttributeReference>& build_keys,
    Vector<BufferPtr>* output_buffers) {
  DCHECK_EQ(project_expressions_.size(), output_buffers->size());

  Vector<const cpp_type*> build_keys_values;
  for (const AttributeReference& build_key : build_keys) {
    ConstBufferPtr build_buffer;
    build_key.Evaluate(build_table.columns(),
                       &build_buffer);
    build_keys_values.emplace_back(build_buffer->as_type<cpp_type>());
  }
  DCHECK_EQ(build_keys_values.size(), probe_key_values_.size());
  switch (build_keys_values.size()) {
    case 1:
      JoinImpl<resizeable, 1, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
    case 2:
      JoinImpl<resizeable, 2, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
    case 3:
      JoinImpl<resizeable, 3, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
    case 4:
      JoinImpl<resizeable, 4, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
    case 5:
      JoinImpl<resizeable, 5, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
    default:
      JoinImpl<resizeable, 6, populate_probe_tids, populate_build_tids>(build_table,
                                                                        hash_table,
                                                                        build_keys_values,
                                                                        output_buffers);
      return;
  }
}

template <bool populate_probe_tids, bool populate_build_tids>
void MultiColumnHashJoin::CollaborateJoin(const TableView& left_build_table,
                                          const TableView& right_build_table,
                                          const FoilHashTable& left_hash_table,
                                          const FoilHashTable& right_hash_table,
                                          const Vector<AttributeReference>& build_keys,
                                          Vector<BufferPtr>* left_output_buffers,
                                          Vector<BufferPtr>* right_output_buffers) {
  DCHECK_EQ(project_expressions_.size(), left_output_buffers->size());
  DCHECK_EQ(project_expressions_.size(), right_output_buffers->size());

  Vector<const cpp_type*> left_build_keys_values;
  Vector<const cpp_type*> right_build_keys_values;
  for (const AttributeReference& build_key : build_keys) {
    ConstBufferPtr left_build_buffer;
    ConstBufferPtr right_build_buffer;

    build_key.Evaluate(left_build_table.columns(),
                       &left_build_buffer);
    build_key.Evaluate(right_build_table.columns(),
                       &right_build_buffer);

    left_build_keys_values.emplace_back(left_build_buffer->as_type<cpp_type>());
    right_build_keys_values.emplace_back(right_build_buffer->as_type<cpp_type>());
  }

  DCHECK_EQ(left_build_keys_values.size(), probe_key_values_.size());
  switch (left_build_keys_values.size()) {
    case 1:
      CollaborateJoinImpl<1, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
    case 2:
      CollaborateJoinImpl<2, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
    case 3:
      CollaborateJoinImpl<3, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
    case 4:
      CollaborateJoinImpl<4, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
    case 5:
      CollaborateJoinImpl<5, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
    default:
      CollaborateJoinImpl<6, populate_probe_tids, populate_build_tids>(left_build_table,
                                                                       right_build_table,
                                                                       left_hash_table,
                                                                       right_hash_table,
                                                                       left_build_keys_values,
                                                                       right_build_keys_values,
                                                                       left_output_buffers,
                                                                       right_output_buffers);
      return;
  }
}

template <bool resizeable, int num_keys, bool populate_probe_tids, bool populate_build_tids>
void MultiColumnHashJoin::JoinImpl(const TableView& build_table,
                                   const FoilHashTable& hash_table,
                                   const Vector<const cpp_type*> build_key_values,
                                   Vector<BufferPtr>* output_buffers) {
  static_assert(populate_build_tids || populate_probe_tids,
                "At least one side of tuple ids needs to be populated");

  const size_type total_num_probe_tuples = probe_table_.num_tuples();
  Vector<const cpp_type*> build_values;
  for (const ConstBufferPtr& build_buffer : build_table.columns()) {
    build_values.emplace_back(build_buffer->as_type<cpp_type>());
  }

  size_type probe_tuple_offset = 0;
  size_type output_offset = 0;

  Vector<const cpp_type*> probe_key_values_block;
  for (const cpp_type* probe_value_ptr : probe_key_values_) {
    probe_key_values_block.emplace_back(probe_value_ptr);
  }

  Vector<const cpp_type*> probe_values_block;
  for (const ConstBufferPtr& probe_buffer : probe_table_.columns()) {
    probe_values_block.emplace_back(probe_buffer->as_type<cpp_type>());
  }

  while (probe_tuple_offset < total_num_probe_tuples) {
    Vector<size_type> probe_tids;
    Vector<size_type> build_tids;
    probe_tids.reserve(FLAGS_join_chunck_size);
    build_tids.reserve(FLAGS_join_chunck_size);
    DoBlockJoin<num_keys, populate_probe_tids, populate_build_tids>(
        std::min(FLAGS_join_chunck_size, total_num_probe_tuples  - probe_tuple_offset),
        probe_key_values_block,
        build_key_values,
        hash_table,
        &probe_tids,
        &build_tids);

    if (resizeable) {
      std::size_t num_join_result;
      if (populate_build_tids) {
        num_join_result = build_tids.size();
      } else {
        num_join_result = probe_tids.size();
      }
      if ((*output_buffers)[0]->num_tuples() < output_offset + num_join_result) {
        const std::size_t new_capacity = std::max(
            static_cast<std::size_t>(output_offset + num_join_result),
            static_cast<std::size_t>((*output_buffers)[0]->num_tuples() * 1.5));
        const std::size_t new_buffer_size = new_capacity * sizeof(cpp_type);
        for (BufferPtr& output_buffer : *output_buffers) {
          output_buffer->Realloc(new_buffer_size, new_capacity);
        }
      }
    }

    for (int i = 0; i < static_cast<int>(project_expressions_.size()); ++i) {
      project_expressions_[i].EvaluateForJoin(probe_values_block,
                                              build_values,
                                              probe_tids,
                                              build_tids,
                                              output_offset,
                                              (*output_buffers)[i].get());
    }

    if (populate_build_tids) {
      output_offset += build_tids.size();
    } else {
      output_offset += probe_tids.size();
    }

    for (const cpp_type*& probe_key_ptr : probe_key_values_block) {
      probe_key_ptr += FLAGS_join_chunck_size;
    }
    for (const cpp_type*& probe_value_ptr : probe_values_block) {
      probe_value_ptr += FLAGS_join_chunck_size;
    }

    probe_tuple_offset += FLAGS_join_chunck_size;
  }

  if (resizeable) {
    const std::size_t actual_buffer_size = output_offset * sizeof(cpp_type);
    for (BufferPtr& output_buffer : *output_buffers) {
      output_buffer->Realloc(actual_buffer_size, output_offset);
    }
  }
}

template <int num_keys, bool populate_probe_tids, bool populate_build_tids>
void MultiColumnHashJoin::DoBlockJoin(
    const size_type num_probe_values,
    const Vector<const cpp_type*>& probe_values,
    const Vector<const cpp_type*>& build_values,
    const FoilHashTable& hash_table,
    Vector<size_type>* probe_tids,
    Vector<size_type>* build_tids) {
  const std::uint32_t mask = hash_table.mask();
  const int* __restrict__ buckets = hash_table.buckets();
  const int* __restrict__ next = hash_table.next();

  for (size_type probe_tid = 0; probe_tid < num_probe_values; ++probe_tid) {
    const hash_type hash_value = Hash<num_keys>(probe_values, probe_tid);
    const int bucket_id = hash_value & mask;

    for (int build_position = buckets[bucket_id] - 1;
         build_position >= 0;
         build_position = next[build_position] - 1) {
      if (VectorEqualAt<num_keys>(probe_values,
                                  build_values,
                                  probe_tid,
                                  build_position)) {
        if (populate_probe_tids) {
          probe_tids->emplace_back(probe_tid);
        }
        if (populate_build_tids) {
          build_tids->emplace_back(build_position);
        }
      }
    }
  }
}

template <int num_keys, bool populate_probe_tids, bool populate_build_tids>
void MultiColumnHashJoin::CollaborateJoinImpl(const TableView& left_build_table,
                                              const TableView& right_build_table,
                                              const FoilHashTable& left_hash_table,
                                              const FoilHashTable& right_hash_table,
                                              const Vector<const cpp_type*>& left_build_key_values,
                                              const Vector<const cpp_type*>& right_build_key_values,
                                              Vector<BufferPtr>* left_output_buffers,
                                              Vector<BufferPtr>* right_output_buffers) {
  static_assert(populate_build_tids || populate_probe_tids,
                "At least one side of tuple ids needs to be populated");

  const size_type total_num_probe_tuples = probe_table_.num_tuples();
  Vector<const cpp_type*> left_build_values;
  Vector<const cpp_type*> right_build_values;
  for (const ConstBufferPtr& build_buffer : left_build_table.columns()) {
    left_build_values.emplace_back(build_buffer->as_type<cpp_type>());
  }
  for (const ConstBufferPtr& build_buffer : right_build_table.columns()) {
    right_build_values.emplace_back(build_buffer->as_type<cpp_type>());
  }

  size_type probe_tuple_offset = 0;
  size_type left_output_offset = 0;
  size_type right_output_offset = 0;

  Vector<const cpp_type*> probe_key_values_block;
  for (const cpp_type* probe_value_ptr : probe_key_values_) {
    probe_key_values_block.emplace_back(probe_value_ptr);
  }

  Vector<const cpp_type*> probe_values_block;
  for (const ConstBufferPtr& probe_buffer : probe_table_.columns()) {
    probe_values_block.emplace_back(probe_buffer->as_type<cpp_type>());
  }

  while (probe_tuple_offset < total_num_probe_tuples) {
    Vector<size_type> left_probe_tids;
    Vector<size_type> left_build_tids;

    left_probe_tids.reserve(FLAGS_join_chunck_size);
    left_build_tids.reserve(FLAGS_join_chunck_size);

    const size_type block_size =
        std::min(FLAGS_join_chunck_size, total_num_probe_tuples  - probe_tuple_offset);
    DoBlockJoin<num_keys, populate_probe_tids, populate_build_tids>(
        block_size,
        probe_key_values_block,
        left_build_key_values,
        left_hash_table,
        &left_probe_tids,
        &left_build_tids);

    Vector<size_type> right_probe_tids;
    Vector<size_type> right_build_tids;
    right_probe_tids.reserve(FLAGS_join_chunck_size);
    right_build_tids.reserve(FLAGS_join_chunck_size);
    DoBlockJoin<num_keys, populate_probe_tids, populate_build_tids>(
        block_size,
        probe_key_values_block,
        right_build_key_values,
        right_hash_table,
        &right_probe_tids,
        &right_build_tids);

    for (int i = 0; i < static_cast<int>(project_expressions_.size()); ++i) {
      project_expressions_[i].EvaluateForJoin(probe_values_block,
                                              left_build_values,
                                              left_probe_tids,
                                              left_build_tids,
                                              left_output_offset,
                                              (*left_output_buffers)[i].get());
      project_expressions_[i].EvaluateForJoin(probe_values_block,
                                              right_build_values,
                                              right_probe_tids,
                                              right_build_tids,
                                              right_output_offset,
                                              (*right_output_buffers)[i].get());
    }

    if (populate_build_tids) {
      left_output_offset += left_build_tids.size();
      right_output_offset += right_build_tids.size();
    } else {
      left_output_offset += left_probe_tids.size();
      right_output_offset += right_probe_tids.size();
    }

    for (const cpp_type*& probe_key_ptr : probe_key_values_block) {
      probe_key_ptr += FLAGS_join_chunck_size;
    }
    for (const cpp_type*& probe_value_ptr : probe_values_block) {
      probe_value_ptr += FLAGS_join_chunck_size;
    }

    probe_tuple_offset += FLAGS_join_chunck_size;
  }
}

}  // namespace quickfoil
