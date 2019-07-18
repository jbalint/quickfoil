/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_SEMI_JOIN_HPP_
#define QUICKFOIL_OPERATIONS_SEMI_JOIN_HPP_

#include <memory>

#include "expressions/AttributeReference.hpp"
#include "memory/Buffer.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"
#include "utility/BitVector.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class FoilHashTable;

struct SemiJoinChunk {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  SemiJoinChunk(Vector<const cpp_type*>&& output_columns_in,
                BitVector&& semi_bitvector_in)
      : output_columns(std::move(output_columns_in)),
        semi_bitvector(std::move(semi_bitvector_in)),
        num_ones(semi_bitvector.count()) {}

  Vector<const cpp_type*> output_columns;
  BitVector semi_bitvector;
  BitVector::size_type num_ones;
};

class SemiJoin {
 public:
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  SemiJoin(const TableView& probe_table,
           const TableView& build_table,
           const FoilHashTable& build_hash_table,
           const Vector<AttributeReference>& probe_keys,
           const Vector<AttributeReference>& build_keys,
           Vector<int>&& project_column_ids)
      : probe_table_(probe_table),
        build_table_(build_table),
        build_hash_table_(build_hash_table),
        project_column_ids_(std::move(project_column_ids)) {
    for (const AttributeReference& probe_key : probe_keys) {
      ConstBufferPtr probe_buffer;
      probe_key.Evaluate(probe_table.columns(),
                         &probe_buffer);
      probe_key_values_.emplace_back(probe_buffer->as_type<cpp_type>());
    }
    for (const AttributeReference& build_key : build_keys) {
      ConstBufferPtr build_buffer;
      build_key.Evaluate(build_table.columns(),
                         &build_buffer);
      build_key_values_.emplace_back(build_buffer->as_type<cpp_type>());
    }
  }

  virtual ~SemiJoin() {}

  virtual SemiJoinChunk* Next() = 0;

 protected:
  const TableView& probe_table_;
  const TableView& build_table_;
  const FoilHashTable& build_hash_table_;
  Vector<const cpp_type*> probe_key_values_;
  Vector<const cpp_type*> build_key_values_;
  Vector<int> project_column_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SemiJoin);
};


}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_SEMI_JOIN_HPP_ */
