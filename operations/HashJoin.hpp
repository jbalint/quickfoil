/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_HASHJOIN_HPP_
#define QUICKFOIL_OPERATIONS_HASHJOIN_HPP_

#include <memory>

#include "expressions/OperatorTraits.hpp"
#include "memory/Buffer.hpp"
#include "schema/TypeDefs.hpp"
#include "operations/PartitionAssigner.hpp"
#include "storage/TableView.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

struct HashJoinChunk {
  HashJoinChunk(int table_id_in,
                int join_group_id_in,
                int partition_id_in,
                size_type binding_partition_size_in,
                const Vector<ConstBufferPtr>& probe_columns_in,
                const Vector<ConstBufferPtr>& build_columns_in,
                Vector<size_type>&& probe_tids_in,
                Vector<size_type>&& build_tids_in,
                Vector<size_type>&& build_relative_tids_in)
      : table_id(table_id_in),
        join_group_id(join_group_id_in),
        partition_id(partition_id_in),
        binding_partition_size(binding_partition_size_in),
        probe_columns(probe_columns_in),
        build_columns(build_columns_in),
        probe_tids(std::move(probe_tids_in)),
        build_tids(std::move(build_tids_in)),
        build_relative_tids(std::move(build_relative_tids_in)) {}

  int table_id;
  int join_group_id;
  int partition_id;
  size_type binding_partition_size;
  const Vector<ConstBufferPtr>& probe_columns;
  const Vector<ConstBufferPtr>& build_columns;
  Vector<size_type> probe_tids;
  Vector<size_type> build_tids;
  Vector<size_type> build_relative_tids;
};

class HashJoin {
 public:
  typedef PartitionAssigner::partition_tuple_type partition_tuple_type;

  HashJoin(const TableView& build_table,
           const int build_column_id,
           PartitionAssigner* assigner)
      : assigner_(assigner),
        build_columns_(build_table.columns()),
        build_hash_tables_(build_table.hash_tables_at(build_column_id)),
        build_partitions_(build_table.partitions_at(build_column_id)) {}

  HashJoinChunk* Next();

 private:
  std::unique_ptr<PartitionAssigner> assigner_;
  const Vector<ConstBufferPtr>& build_columns_;
  const Vector<FoilHashTable>& build_hash_tables_;
  const Vector<ConstBufferPtr>& build_partitions_;

  OperatorTraits<OperatorType::kEqual>::op equality_operator_;

  DISALLOW_COPY_AND_ASSIGN(HashJoin);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_HASHJOIN_HPP_ */
