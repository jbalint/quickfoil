/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_STORAGE_TABLE_VIEW_HPP_
#define QUICKFOIL_STORAGE_TABLE_VIEW_HPP_

#include <memory>
#include <utility>

#include "memory/Buffer.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/FoilHashTable.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class TableView {
 public:
  TableView(Vector<ConstBufferPtr>&& columns)
      : columns_(std::move(columns)),
        partitions_(columns_.size()),
        hash_tables_(columns_.size()) {
    DCHECK(!columns_.empty());
  }

  TableView(const Vector<ConstBufferPtr>& columns)
      : columns_(columns),
        partitions_(columns_.size()),
        hash_tables_(columns_.size()) {
    DCHECK(!columns_.empty());
  }

  const Vector<ConstBufferPtr>& columns() const {
    return columns_;
  }

  const ConstBufferPtr& column_at(int i) const {
    return columns_[i];
  }

  TableView* Clone() const {
    return new TableView(columns_);
  }

  int num_columns() const {
    return columns_.size();
  }

  bool empty() const {
    return columns_[0]->num_tuples() == 0u;
  }

  size_type num_tuples() const {
    return columns_[0]->num_tuples();
  }

  void set_partitions_at(int column_id,
                         Vector<ConstBufferPtr>&& partitions) {
    DCHECK(partitions_[column_id].empty());
    partitions_[column_id] = std::move(partitions);
  }

  const Vector<ConstBufferPtr>& partitions_at(int column_id) const {
    return partitions_[column_id];
  }

  void set_hash_tables_at(int column_id,
                          Vector<FoilHashTable>&& hash_tables) {
    DCHECK(hash_tables_[column_id].empty());
    hash_tables_[column_id] = std::move(hash_tables);
  }

  const Vector<FoilHashTable>& hash_tables_at(int column_id) const {
    return hash_tables_[column_id];
  }

 private:
  Vector<ConstBufferPtr> columns_;
  Vector<Vector<ConstBufferPtr>> partitions_;
  Vector<Vector<FoilHashTable>> hash_tables_;

  DISALLOW_COPY_AND_ASSIGN(TableView);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_STORAGE_TABLE_VIEW_HPP_ */
