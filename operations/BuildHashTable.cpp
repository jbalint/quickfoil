/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "operations/BuildHashTable.hpp"

#include <memory>
#include <utility>

#include "expressions/AttributeReference.hpp"
#include "operations/SemiJoin.hpp"
#include "storage/FoilHashTable.hpp"
#include "storage/PartitionTuple.hpp"
#include "storage/TableView.hpp"
#include "utility/BitVector.hpp"
#include "utility/BitVectorIterator.hpp"
#include "utility/Hash.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace quickfoil {

DECLARE_int32(num_radix_bits);

template <typename cpp_type>
FoilHashTable* BuildHashTable(int num_radix_bits,
                              size_type num_tuples,
                              const cpp_type* __restrict__ values) {
  DCHECK_GT(num_tuples, 0);

  std::unique_ptr<FoilHashTable> hash_table(new FoilHashTable(num_tuples,
                                                              num_radix_bits));

  const uint32_t mask = hash_table->mask();
  int* __restrict__ buckets = hash_table->mutable_buckets();
  int* __restrict__ next = hash_table->mutable_next();

  for (size_type index = 0; index < num_tuples; ++index) {
    const hash_type hash_value = Hash(*values);
    const int bucket_id = HASH_BIT_MODULO(hash_value, mask, num_radix_bits);
    DCHECK_LT(static_cast<size_t>(bucket_id), (mask >> num_radix_bits) + 1);
    *next = buckets[bucket_id];
    buckets[bucket_id] = index + 1;
    ++values;
    ++next;
  }

  return hash_table.release();
}

void BuildHashTableOnPartitions(
    const int column_id,
    TableView* table) {
  const int num_radix_bits = FLAGS_num_radix_bits;
  const Vector<ConstBufferPtr>& partitions = table->partitions_at(column_id);
  DCHECK(!partitions.empty());
  DCHECK(table->hash_tables_at(column_id).empty());

  Vector<FoilHashTable> hash_tables;
  hash_tables.reserve(partitions.size());
  for (const ConstBufferPtr& partition : partitions) {
    const size_type num_tuples = partition->num_tuples();
    if (num_tuples == 0) {
      hash_tables.emplace_back();
      continue;
    }

    const PartitionTuple<kQuickFoilDefaultDataType>* __restrict__ partition_tuples =
        partition->as_type<PartitionTuple<kQuickFoilDefaultDataType>>();

    hash_tables.emplace_back(num_tuples,
                             num_radix_bits);

    const std::uint32_t mask = hash_tables.back().mask();
    int* __restrict__ buckets = hash_tables.back().mutable_buckets();
    int* __restrict__ next = hash_tables.back().mutable_next();

    for (size_type index = 0; index < num_tuples; ++index) {
      const hash_type hash_value = Hash(partition_tuples->value);
      const int bucket_id = HASH_BIT_MODULO(hash_value, mask, num_radix_bits);
      DCHECK_LT(static_cast<size_t>(bucket_id), (mask >> num_radix_bits) + 1);
      *next = buckets[bucket_id];
      buckets[bucket_id] = index + 1;
      ++partition_tuples;
      ++next;
    }
  }

  table->set_hash_tables_at(column_id, std::move(hash_tables));
}

namespace {

template <int num_keys>
FoilHashTable* BuildHashTableOnTableImpl(
    const Vector<AttributeReference>& build_keys,
    const TableView& table) {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;
  Vector<const cpp_type*> build_keys_values;
  for (const AttributeReference& build_key : build_keys) {
    ConstBufferPtr build_buffer;
    build_key.Evaluate(table.columns(), &build_buffer);
    build_keys_values.emplace_back(build_buffer->as_type<cpp_type>());
  }

  const size_type num_tuples = table.num_tuples();
  std::unique_ptr<FoilHashTable> hash_table(new FoilHashTable(num_tuples,
                                                              0));
  const std::uint32_t mask = hash_table->mask();
  int* __restrict__ buckets = hash_table->mutable_buckets();
  int* __restrict__ next = hash_table->mutable_next();

  for (size_type index = 0; index < num_tuples; ++index) {
    const hash_type hash_value = Hash<num_keys>(build_keys_values, index);
    const int bucket_id = hash_value & mask;
    *next = buckets[bucket_id];
    buckets[bucket_id] = index + 1;
    ++next;
  }

  return hash_table.release();
}

template <int num_keys, typename T>
inline void InsertIfNotPresent(const std::uint32_t mask,
                               const Vector<const T*>& build_keys_values,
                               const size_type tid,
                               int* __restrict__ buckets,
                               int* __restrict__ next) {
  const int bucket_id = Hash<num_keys>(build_keys_values, tid) & mask;
  bool exist = false;
  for (int build_position = buckets[bucket_id] - 1;
       build_position >= 0;
       build_position = next[build_position] - 1) {
    if (VectorEqualAt<num_keys>(build_keys_values,
                                build_keys_values,
                                tid,
                                build_position)) {
      exist = true;
      break;
    }
  }
  if (!exist) {
    next[tid] = buckets[bucket_id];
    buckets[bucket_id] = tid + 1 ;
  }
}

template <int num_keys>
FoilHashTable* BuildHashTableAfterSemiJoinImpl(
    const size_type num_build_tuples,
    SemiJoin* semi_join_in) {
  typedef SemiJoin::cpp_type cpp_type;

  std::unique_ptr<SemiJoin> semi_join(semi_join_in);
  std::unique_ptr<FoilHashTable> hash_table(
      new FoilHashTable(num_build_tuples, 0));

  const std::uint32_t mask = hash_table->mask();
  int* buckets = hash_table->mutable_buckets();
  int* next = hash_table->mutable_next();

  std::unique_ptr<SemiJoinChunk> semi_join_result(semi_join->Next());
  while (semi_join_result != nullptr) {
    const size_type num_result = semi_join_result->num_ones;
    if (num_result > 0) {
      const Vector<const cpp_type*>& build_keys_values = semi_join_result->output_columns;
      BitVectorIterator bv_it(semi_join_result->semi_bitvector);
      InsertIfNotPresent<num_keys>(mask,
                                   build_keys_values,
                                   bv_it.GetFirst(),
                                   buckets,
                                   next);
      for (size_type i = 1; i < num_result; ++i) {
        InsertIfNotPresent<num_keys>(mask,
                                     build_keys_values,
                                     bv_it.FindNext(),
                                     buckets,
                                     next);
      }
    }
    semi_join_result.reset(semi_join->Next());
  }

  return hash_table.release();
}

}  // namespace

FoilHashTable* BuildHashTableOnTable(
    const Vector<AttributeReference>& build_keys,
    const TableView& table) {
  switch (build_keys.size()) {
    case 1:
      return BuildHashTableOnTableImpl<1>(build_keys, table);
    case 2:
      return BuildHashTableOnTableImpl<2>(build_keys, table);
    case 3:
      return BuildHashTableOnTableImpl<3>(build_keys, table);
    case 4:
      return BuildHashTableOnTableImpl<4>(build_keys, table);
    case 5:
      return BuildHashTableOnTableImpl<5>(build_keys, table);
    default:
      return BuildHashTableOnTableImpl<6>(build_keys, table);
  }
}

FoilHashTable* BuildHashTableAfterSemiJoin(
    const size_type num_build_tuples,
    int num_build_keys,
    SemiJoin* semi_join) {
  switch (num_build_keys) {
    case 1:
      return BuildHashTableAfterSemiJoinImpl<1>(num_build_tuples,
                                                semi_join);
    case 2:
      return BuildHashTableAfterSemiJoinImpl<2>(num_build_tuples,
                                                semi_join);
    case 3:
      return BuildHashTableAfterSemiJoinImpl<3>(num_build_tuples,
                                                semi_join);
    case 4:
      return BuildHashTableAfterSemiJoinImpl<4>(num_build_tuples,
                                                semi_join);
    case 5:
      return BuildHashTableAfterSemiJoinImpl<5>(num_build_tuples,
                                                semi_join);
    default:
      return BuildHashTableAfterSemiJoinImpl<6>(num_build_tuples,
                                                semi_join);
  }
}

}  // namespace quickfoil
