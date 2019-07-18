/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_PARTITION_ASSIGNER_HPP_
#define QUICKFOIL_OPERATIONS_PARTITION_ASSIGNER_HPP_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "memory/Buffer.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "storage/PartitionTuple.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"

namespace quickfoil {

DECLARE_int32(partition_chunck_size);

struct PartitionChunk {
  PartitionChunk(int table_id_in,
                 int join_group_id_in,
                 int partition_id_in,
                 ConstBufferPtr&& partition_in,
                 const Vector<ConstBufferPtr>& columns_in)
      : table_id(table_id_in),
        join_group_id(join_group_id_in),
        partition_id(partition_id_in),
        partition(std::move(partition_in)),
        columns(columns_in) {}

  int table_id;
  int join_group_id;
  int partition_id;
  ConstBufferPtr partition;
  const Vector<ConstBufferPtr>& columns;
};

class PartitionAssigner {
 public:
  typedef PartitionTuple<kQuickFoilDefaultDataType> partition_tuple_type;

  PartitionAssigner(Vector<const TableView*>&& tables,
                    Vector<Vector<int>>&& partition_column_ids)
      : tables_(std::move(tables)),
        partition_column_ids_(std::move(partition_column_ids)),
        cur_table_id_(0),
        cur_join_group_id_(0),
        cur_partition_id_(0),
        cur_partition_offset_(0) {
    cur_partitions_ =
        &tables_[0]->partitions_at(partition_column_ids_[0][0]);
    num_partitions_ = cur_partitions_->size();
  }

  PartitionAssigner(const Vector<const TableView*>& tables,
                    const Vector<Vector<int>>& partition_column_ids)
      : tables_(tables),
        partition_column_ids_(partition_column_ids),
        cur_table_id_(0),
        cur_join_group_id_(0),
        cur_partition_id_(0),
        cur_partition_offset_(0) {
    cur_partitions_ =
        &tables_[0]->partitions_at(partition_column_ids_[0][0]);
    num_partitions_ = cur_partitions_->size();
  }

  PartitionChunk* Next() {
    do {
      START_TIMER(QuickFoilTimer::kAssigner);
      while (cur_partition_offset_ == (*cur_partitions_)[cur_partition_id_]->num_tuples()) {
        if (MoveToNextJoinGroup()) {
          STOP_TIMER(QuickFoilTimer::kAssigner);
          return nullptr;
        }
      }

      const ConstBufferPtr& cur_partition = (*cur_partitions_)[cur_partition_id_];
      const std::size_t num_partition_tuples =
          std::min(static_cast<std::size_t>(FLAGS_partition_chunck_size),
                   cur_partition->num_tuples() - cur_partition_offset_);
      ConstBufferPtr partition_chunk(std::move(
          std::make_shared<const ConstBuffer>(
              cur_partition,
              cur_partition->as_type<partition_tuple_type>() + cur_partition_offset_,
              num_partition_tuples)));
      cur_partition_offset_ += num_partition_tuples;
      STOP_TIMER(QuickFoilTimer::kAssigner);
      return new PartitionChunk(cur_table_id_,
                                cur_join_group_id_,
                                cur_partition_id_,
                                std::move(partition_chunk),
                                tables_[cur_table_id_]->columns());
    } while (true);
  }

 private:
  bool MoveToNextJoinGroup() {
    ++cur_join_group_id_;
    if (cur_join_group_id_ == partition_column_ids_[cur_table_id_].size() &&
        MoveToNextJoinTable()) {
      return true;
    }
    cur_partitions_ =
        &tables_[cur_table_id_]->partitions_at(
            partition_column_ids_[cur_table_id_][cur_join_group_id_]);
    cur_partition_offset_ = 0;
    return false;
  }

  bool MoveToNextJoinTable() {
    ++cur_table_id_;
    cur_join_group_id_ = 0;
    if (cur_table_id_ == tables_.size() && MoveToNextPartition()) {
      return true;
    }
    return false;
  }

  bool MoveToNextPartition() {
    ++cur_partition_id_;
    cur_table_id_ = 0;
    cur_join_group_id_ = 0;

    if (cur_partition_id_ == num_partitions_) {
      return true;
    }
    return false;
  }

  const Vector<const TableView*>& tables_;
  Vector<Vector<int>> partition_column_ids_;

  std::size_t num_partitions_;
  std::size_t cur_table_id_;
  std::size_t cur_join_group_id_;
  std::size_t cur_partition_id_;
  std::size_t cur_partition_offset_;
  const Vector<ConstBufferPtr>* cur_partitions_;

  DISALLOW_COPY_AND_ASSIGN(PartitionAssigner);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_PARTITION_ASSIGNER_HPP_ */
