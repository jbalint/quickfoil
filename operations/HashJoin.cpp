/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/HashJoin.hpp"

#include <memory>

#include "learner/QuickFoilTimer.hpp"
#include "operations/PartitionAssigner.hpp"
#include "storage/FoilHashTable.hpp"
#include "utility/Hash.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"

namespace quickfoil {

DECLARE_int32(num_radix_bits);

HashJoinChunk* HashJoin::Next() {
  std::unique_ptr<const PartitionChunk> partition_chunk;
  Vector<size_type> probe_tids;
  Vector<size_type> build_tids;
  Vector<size_type> build_relative_tids;

  do {
    partition_chunk.reset(assigner_->Next());
    if (partition_chunk == nullptr) {
      return nullptr;
    }

    const partition_tuple_type* __restrict__ probe_partition =
        partition_chunk->partition->as_type<partition_tuple_type>();
    if (build_partitions_[partition_chunk->partition_id]->num_tuples() == 0) {
      continue;
    }

    START_TIMER(QuickFoilTimer::kHashJoin);
    const partition_tuple_type* build_partition =
        build_partitions_[partition_chunk->partition_id]->as_type<partition_tuple_type>();
    const FoilHashTable& build_hash_table = build_hash_tables_[partition_chunk->partition_id];

    const int* __restrict__ next = build_hash_table.next();
    const int* __restrict__ buckets = build_hash_table.buckets();

    std::uint32_t mask = build_hash_table.mask();
    const std::size_t num_tuples = partition_chunk->partition->num_tuples();

    probe_tids.reserve(num_tuples);
    build_tids.reserve(num_tuples);
    build_relative_tids.reserve(num_tuples);
    for (std::size_t tid = 0; tid < num_tuples; ++tid) {
      const hash_type hash_value = Hash(probe_partition->value);
      const int bucket_id = (hash_value & mask) >> FLAGS_num_radix_bits;
      for (int build_partition_position = buckets[bucket_id] - 1;
           build_partition_position >= 0;
           build_partition_position = next[build_partition_position] - 1) {
        DCHECK_LT(build_partition_position,
                  static_cast<int>(build_partitions_[partition_chunk->partition_id]->num_tuples()));
        if (equality_operator_(build_partition[build_partition_position].value,
                               probe_partition->value)) {
          build_tids.emplace_back(build_partition[build_partition_position].tuple_id);
          probe_tids.emplace_back(probe_partition->tuple_id);
          build_relative_tids.emplace_back(build_partition_position);
        }
      }
      ++probe_partition;
    }

    STOP_TIMER(QuickFoilTimer::kHashJoin);

  } while (build_tids.empty());

  return new HashJoinChunk(partition_chunk->table_id,
                           partition_chunk->join_group_id,
                           partition_chunk->partition_id,
                           build_partitions_[partition_chunk->partition_id]->num_tuples(),
                           partition_chunk->columns,
                           build_columns_,
                           std::move(probe_tids),
                           std::move(build_tids),
                           std::move(build_relative_tids));
}

}  // namespace quickfoil
