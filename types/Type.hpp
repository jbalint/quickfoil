/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_TYPES_TYPE_HPP_
#define QUICKFOIL_TYPES_TYPE_HPP_

#include <cstddef>
#include <cmath>

#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"

namespace quickfoil {

class Type;
class TypeResolver;

const Type& GetType(TypeID type_id);

class Type {
 public:
  TypeID type_id() const {
    return type_id_;
  }

  const std::string& name() const {
    return name_;
  }

  std::size_t size() const {
    return size_;
  }

  bool is_variable_length() const {
    return is_variable_length_;
  }

  bool is_numeric() const {
    return is_numeric_;
  }

  bool EqualTo(const Type& other) const {
    return this == &other;
  }

 private:
  template <typename TypeTraits>
  Type(TypeTraits ignored)
      : type_id_(TypeTraits::id),
        size_(TypeTraits::size),
        is_variable_length_(TypeTraits::variable_length_data),
        is_numeric_(TypeTraits::is_numeric),
        name_(TypeTraits::name) {
  }

  friend class TypeResolver;

  TypeID type_id_;
  std::size_t size_;
  bool is_variable_length_;
  bool is_numeric_;
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Type)
  ;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_TYPES_TYPE_HPP_ */
