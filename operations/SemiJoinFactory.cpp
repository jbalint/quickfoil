/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/SemiJoinFactory.hpp"

#include "operations/BuildHashTable.hpp"
#include "operations/LeftSemiJoin.hpp"
#include "operations/RightSemiJoin.hpp"
#include "storage/TableView.hpp"

#include "glog/logging.h"

namespace quickfoil {
namespace {

template <int num_keys>
SemiJoin* CreateSemiJoinHelper(bool left_semijoin,
                               const TableView& probe_table,
                               const TableView& build_table,
                               const FoilHashTable& build_hash_table,
                               const Vector<AttributeReference>& probe_keys,
                               const Vector<AttributeReference>& build_keys,
                               Vector<int>&& project_column_ids) {
  if (left_semijoin) {
    return new LeftSemiJoin<num_keys>(probe_table,
                                      build_table,
                                      build_hash_table,
                                      probe_keys,
                                      build_keys,
                                      std::move(project_column_ids));
  } else {
    return new RightSemiJoin<num_keys>(probe_table,
                                       build_table,
                                       build_hash_table,
                                       probe_keys,
                                       build_keys,
                                       std::move(project_column_ids));
  }
}

}  // namespace

SemiJoin* SelectAndCreateSemiJoin(
    const TableView& output_table,
    const TableView& other_table,
    std::unique_ptr<FoilHashTable>* output_hash_table,
    std::unique_ptr<FoilHashTable>* other_hash_table,
    const Vector<AttributeReference>& output_join_keys,
    const Vector<AttributeReference>& other_join_keys,
    const Vector<int>& project_column_ids) {
  if (output_table.num_tuples() < other_table.num_tuples()) {
    // right semijoin
    if (*output_hash_table == nullptr) {
      output_hash_table->reset(
          BuildHashTableOnTable(output_join_keys,
                                output_table));
    }

    return CreateSemiJoin(false,
                          other_table,
                          output_table,
                          **output_hash_table,
                          other_join_keys,
                          output_join_keys,
                          std::move(project_column_ids));
  }

  if (*other_hash_table == nullptr) {
    other_hash_table->reset(
        BuildHashTableOnTable(other_join_keys,
                              other_table));
  }
  return CreateSemiJoin(true,
                        output_table,
                        other_table,
                        **other_hash_table,
                        output_join_keys,
                        other_join_keys,
                        std::move(project_column_ids));
}

SemiJoin* CreateSemiJoin(bool left_semijoin,
                         const TableView& probe_table,
                         const TableView& build_table,
                         const FoilHashTable& build_hash_table,
                         const Vector<AttributeReference>& probe_keys,
                         const Vector<AttributeReference>& build_keys,
                         const Vector<int>& project_column_ids) {
  DCHECK_EQ(probe_keys.size(), build_keys.size());
  Vector<int> project_column_ids_clone = project_column_ids;
  switch (probe_keys.size()) {
    case 1:
      return CreateSemiJoinHelper<1>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
    case 2:
      return CreateSemiJoinHelper<2>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
    case 3:
      return CreateSemiJoinHelper<3>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
    case 4:
      return CreateSemiJoinHelper<4>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
    case 5:
      return CreateSemiJoinHelper<5>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
    default:
      return CreateSemiJoinHelper<6>(left_semijoin,
                                     probe_table,
                                     build_table,
                                     build_hash_table,
                                     probe_keys,
                                     build_keys,
                                     std::move(project_column_ids_clone));
  }
}

}  // namespace quickfoil
