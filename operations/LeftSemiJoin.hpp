/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_LEFT_SEMI_JOIN_HPP_
#define QUICKFOIL_OPERATIONS_LEFT_SEMI_JOIN_HPP_

#include "operations/SemiJoin.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/FoilHashTable.hpp"
#include "utility/BitVector.hpp"
#include "utility/BitVectorBuilder.hpp"
#include "utility/Hash.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"

namespace quickfoil {

DECLARE_int32(semijoin_chunck_size);

template <int num_keys>
class LeftSemiJoin : public SemiJoin {
 public:
  LeftSemiJoin(const TableView& probe_table,
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
        total_probe_tuples_(probe_table.num_tuples()),
        cur_probe_offset_(0) {
  }

  SemiJoinChunk* Next() override;

 private:
  void DoSemiJoin(const size_type num_probe_tuples,
                  const Vector<const cpp_type*>& probe_table,
                  BitVector* semi_bitvector);

  size_type total_probe_tuples_;
  size_type cur_probe_offset_;
  DISALLOW_COPY_AND_ASSIGN(LeftSemiJoin);
};

template <int num_keys>
SemiJoinChunk* LeftSemiJoin<num_keys>::Next() {
  if (cur_probe_offset_ >= total_probe_tuples_) {
    return nullptr;
  }

  const size_type num_tuples = std::min(FLAGS_semijoin_chunck_size,
                                        total_probe_tuples_ - cur_probe_offset_);

  Vector<const cpp_type*> result_columns;
  BitVector bit_vector(num_tuples);

  Vector<const cpp_type*> probe_key_values_block;
  for (const cpp_type* probe_key_values : probe_key_values_) {
    probe_key_values_block.emplace_back(
        probe_key_values + cur_probe_offset_);
  }

  DoSemiJoin(num_tuples,
             probe_key_values_block,
             &bit_vector);

  for (int column_id : project_column_ids_) {
    result_columns.emplace_back(
        probe_table_.column_at(column_id)->template as_type<cpp_type>() + cur_probe_offset_);
  }
  cur_probe_offset_ += FLAGS_semijoin_chunck_size;

  return new SemiJoinChunk(std::move(result_columns), std::move(bit_vector));
}

template <int num_keys>
void LeftSemiJoin<num_keys>::DoSemiJoin(
    const size_type num_probe_tuples,
    const Vector<const cpp_type*>& probe_key_values,
    BitVector* semi_bitvector) {
  const std::uint32_t mask = build_hash_table_.mask();
  const int* __restrict__ buckets = build_hash_table_.buckets();
  const int* __restrict__ next = build_hash_table_.next();

  BitVectorBuilder result_builder(semi_bitvector);
  BitVectorBuilder::buffer_type::iterator raw_bit_vector_iterator =
      result_builder.bit_vector()->begin();
  size_type probe_tid = 0;
  for (std::size_t block_id = 0; block_id < result_builder.num_blocks(); ++block_id) {
    for (unsigned bit = 0; bit < 64; ++bit) {
      const hash_type hash_value = Hash<num_keys>(probe_key_values, probe_tid);
      const int bucket_id = hash_value & mask;
      bool has_match = false;
      for (int build_position = buckets[bucket_id] - 1;
           build_position >= 0;
           build_position = next[build_position] - 1) {
        if (VectorEqualAt<num_keys>(probe_key_values,
                                    build_key_values_,
                                    probe_tid,
                                    build_position)) {
          has_match = true;
          break;
        }
      }
      *raw_bit_vector_iterator |=
          (static_cast<BitVectorBuilder::block_type>(has_match) << bit);
      ++probe_tid;
    }
    ++raw_bit_vector_iterator;
  }

  for (unsigned bit = 0; bit < result_builder.bits_in_last_block(); ++bit) {
    const hash_type hash_value = Hash<num_keys>(probe_key_values, probe_tid);
    const int bucket_id = hash_value & mask;
    bool has_match = false;
    for (int build_position = buckets[bucket_id] - 1;
         build_position >= 0;
         build_position = next[build_position] - 1) {
      if (VectorEqualAt<num_keys>(probe_key_values,
                                  build_key_values_,
                                  probe_tid,
                                  build_position)) {
        has_match = true;
        break;
      }
    }
    *raw_bit_vector_iterator |=
        (static_cast<BitVectorBuilder::block_type>(has_match) << bit);
    ++probe_tid;
  }
}

}

#endif /* QUICKFOIL_OPERATIONS_LEFT_SEMI_JOIN_HPP_ */
