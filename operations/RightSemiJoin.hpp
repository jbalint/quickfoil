/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_RIGHT_SEMI_JOIN_HPP_
#define QUICKFOIL_OPERATIONS_RIGHT_SEMI_JOIN_HPP_

#include "operations/SemiJoin.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/FoilHashTable.hpp"
#include "utility/BitVector.hpp"
#include "utility/Hash.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

template <int num_keys>
class RightSemiJoin : public SemiJoin {
 public:
  RightSemiJoin(const TableView& probe_table,
                const TableView& build_table,
                const FoilHashTable& build_hash_table,
                const Vector<AttributeReference>& probe_keys,
                const Vector<AttributeReference>& build_keys,
                Vector<int>&& project_column_ids)
      : SemiJoin(probe_table,
                 build_table,
                 build_hash_table,
                 probe_keys,
                 build_keys,
                 std::move(project_column_ids)),
        finished_(false) {}

  SemiJoinChunk* Next() override;

 private:
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(RightSemiJoin);
};

template <int num_keys>
SemiJoinChunk* RightSemiJoin<num_keys>::Next() {
  if (finished_) {
    return nullptr;
  }

  finished_ = true;

  const size_type num_build_tuples = build_table_.num_tuples();

  BitVector bit_vector(num_build_tuples);

  const std::uint32_t mask = build_hash_table_.mask();
  const int* __restrict__ buckets = build_hash_table_.buckets();
  const int* __restrict__ next = build_hash_table_.next();
  const size_type num_probe_tuples = probe_table_.num_tuples();
  for (size_type probe_tid = 0; probe_tid < num_probe_tuples; ++probe_tid) {
    const hash_type hash_value = Hash<num_keys>(probe_key_values_, probe_tid);
    const int bucket_id = hash_value & mask;
    for (int build_position = buckets[bucket_id] - 1;
         build_position >= 0;
         build_position = next[build_position] - 1) {
      if (VectorEqualAt<num_keys>(probe_key_values_,
                                  build_key_values_,
                                  probe_tid,
                                  build_position)) {
        bit_vector.test_set(build_position);
      }
    }
  }

  Vector<const cpp_type*> output_columns;
  for (int column_id : project_column_ids_) {
    output_columns.emplace_back(
        build_table_.column_at(column_id)->template as_type<cpp_type>());
  }
  return new SemiJoinChunk(std::move(output_columns),
                           std::move(bit_vector));
}

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_RIGHT_SEMI_JOIN_HPP_ */
