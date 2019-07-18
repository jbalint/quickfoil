/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_TYPES_FROM_STRING_HPP_
#define QUICKFOIL_TYPES_FROM_STRING_HPP_

#include <string>

#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"

#include "folly/Range.h"
#include "glog/logging.h"

namespace quickfoil {

// <value> does not own the out-of-line data.
template<TypeID type_id>
void FromString(const folly::StringPiece str, typename TypeTraits<type_id>::cpp_type* value) {}

template<>
void FromString<kInt32>(const folly::StringPiece str, TypeTraits<kInt32>::cpp_type* value) {
  try {
    *value = std::stoi(str.data());
  } catch (const std::exception& ex) {
    LOG(FATAL) << "Conversion failed: " << ex.what();
  }
}

template<>
void FromString<kInt64>(const folly::StringPiece str, TypeTraits<kInt64>::cpp_type* value) {
  try {
    *value = std::atoll(str.data());
  } catch (const std::exception& ex) {
    LOG(FATAL) << "Conversion failed: " << ex.what();
  }
}

template<>
void FromString<kDouble>(const folly::StringPiece str, TypeTraits<kDouble>::cpp_type* value) {
  try {
    *value = std::stod(str.data());
  } catch (const std::exception& ex) {
    LOG(FATAL) << "Conversion failed: " << ex.what();
  }
}

template<>
void FromString<kString>(const folly::StringPiece str, TypeTraits<kString>::cpp_type* value) {
  *value = TypeTraits<kString>::cpp_type(str.data());
}

}  // namespace quickfoil

#endif /* QUICKFOIL_TYPES_FROM_STRING_HPP_ */
