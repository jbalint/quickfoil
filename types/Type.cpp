/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "types/Type.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"

#include "glog/logging.h"

namespace quickfoil {

class TypeResolver {
 public:
  const Type& GetBasicType(TypeID type) {
    DCHECK(basic_types_map_.find(type) != basic_types_map_.end());
    return *basic_types_map_.find(type)->second;
  }

  static TypeResolver* GetSingleton() {
    static TypeResolver resolver;
    return &resolver;
  }

 private:
  TypeResolver() {
    AddBasicType<kInt32>();
    AddBasicType<kInt64>();
    AddBasicType<kDouble>();
    AddBasicType<kString>();
  }

  template<TypeID basic_type_id>
  void AddBasicType() {
    TypeTraits<basic_type_id> basic_type_traits;
    const Type* basic_type = new Type(basic_type_traits);
    basic_types_map_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(basic_type_id),
                             std::forward_as_tuple(basic_type));
  }

  std::unordered_map<TypeID,
                     std::unique_ptr<const Type>, std::hash<std::size_t>> basic_types_map_;

  DISALLOW_COPY_AND_ASSIGN(TypeResolver);
};

const Type& GetType(TypeID type_id) {
  return TypeResolver::GetSingleton()->GetBasicType(type_id);
}

}  // namespace quickfoil
