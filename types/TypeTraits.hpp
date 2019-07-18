/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_TYPES_TYPES_TRAITS_HPP_
#define QUICKFOIL_TYPES_TYPES_TRAITS_HPP_

#include <cstdint>

#include "types/TypeID.hpp"

#include "folly/Range.h"

namespace quickfoil {

template<TypeID type_id> struct TypeTraits {};

struct BasicNumericTypeTraits {
  static constexpr bool variable_length_data = false;
  static constexpr bool is_numeric = true;
};

template<> struct TypeTraits<kInt32> : public BasicNumericTypeTraits {
  typedef int32_t cpp_type;

  static constexpr TypeID id = kInt32;
  static constexpr int size = sizeof(cpp_type);
  static constexpr const char* name = "kInt32";
};

template<> struct TypeTraits<kInt64> : public BasicNumericTypeTraits {
  typedef int64_t cpp_type;

  static constexpr TypeID id = kInt64;
  static constexpr int size = sizeof(cpp_type);
  static constexpr const char* name = "kInt64";
};

template<> struct TypeTraits<kDouble> : public BasicNumericTypeTraits {
  typedef double cpp_type;

  static constexpr TypeID id = kDouble;
  static constexpr int size = sizeof(cpp_type);
  static constexpr const char* name = "kDouble";
};

template<> struct TypeTraits<kString> {
  typedef folly::StringPiece cpp_type;

  static constexpr TypeID id = kString;
  static constexpr int size = sizeof(cpp_type);

  static constexpr bool variable_length_data = true;
  static constexpr bool is_numeric = false;
  static constexpr const char* name = "kString";
};

}  // namespace quickfoil

#endif /* QUICKFOIL_TYPES_TYPES_TRAITS_HPP_ */
