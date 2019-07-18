/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_STORAGE_PARTITION_TUPLE_HPP_
#define QUICKFOIL_STORAGE_PARTITION_TUPLE_HPP_

#include <sstream>

#include "schema/TypeDefs.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"

#include "glog/logging.h"

namespace quickfoil {


template <TypeID type_id>
struct PartitionTuple {
  typedef typename TypeTraits<type_id>::cpp_type cpp_type;

  PartitionTuple(cpp_type value_in, size_type tuple_id_in)
      : value(value_in), tuple_id(tuple_id_in) {}

  bool operator==(const PartitionTuple<type_id>& other) const {
    DCHECK(tuple_id != other.tuple_id || value == other.value)
        << tuple_id << " " << other.tuple_id << " "
        << value << " " << other.value;
    return tuple_id == other.tuple_id;
  }

  std::string ToString() const {
    std::ostringstream out;
    out <<"(" << value <<", " << tuple_id <<")";
    return out.str();
  }

  cpp_type value;
  size_type tuple_id;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_STORAGE_PARTITION_TUPLE_HPP_ */
