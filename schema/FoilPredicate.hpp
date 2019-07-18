/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_SCHEMA_FOILPREDICATE_HPP_
#define QUICKFOIL_SCHEMA_FOILPREDICATE_HPP_

#include <string>

#include "memory/Buffer.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/TableView.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class FoilPredicate {
 public:
  // Takes ownership of <columns>.
  FoilPredicate(int id,
                const std::string& name,
                int key,
                const Vector<int>& argument_types,
                Vector<ConstBufferPtr>&& columns)
      : id_(id),
        name_(name),
        key_(key),
        argument_types_(argument_types),
        fact_table_(std::move(columns)) {
  }

  const std::string& name() const { return name_; }

  int id() const {
    return id_;
  }

  const TableView& fact_table() const {
    return fact_table_;
  }

  TableView* mutable_fact_table() {
    return &fact_table_;
  }

  const Vector<int>& argument_types() const {
    return argument_types_;
  }

  int argument_type_at(int index) const {
    return argument_types_[index];
  }

  int num_arguments() const {
    return argument_types_.size();
  }

  size_type GetNumTotalFacts() const {
    return fact_table_.num_tuples();
  }

  int key() const {
    return key_;
  }

 private:
  int id_;
  std::string name_;
  // -1 if there is no key.
  int key_;
  Vector<int> argument_types_;
  TableView fact_table_;

  DISALLOW_COPY_AND_ASSIGN(FoilPredicate);
};

} /* namespace quickfoil */

#endif /* QUICKFOIL_SCHEMA_FOILPREDICATE_HPP_ */
