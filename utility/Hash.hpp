/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_HASH_HPP_
#define QUICKFOIL_UTILITY_HASH_HPP_

#include <functional>

#include "schema/TypeDefs.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

typedef uint32_t hash_type;

#define HASH_BIT_MODULO(hash_value, mask, bits) (((hash_value) & mask) >> bits)

template <class T>
inline hash_type Hash(const T value) {
  return static_cast<hash_type>(std::hash<T>()(value));
}

template <class T>
inline hash_type HashCombine(const hash_type seed, const T value) {
  return static_cast<hash_type>(seed ^ (Hash(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
}

template <>
inline hash_type Hash(const folly::StringPiece value) {
  return static_cast<hash_type>(value.hash());
}

template <int num_values, class T>
typename std::enable_if<num_values < 6, hash_type>::type Hash(const Vector<const T*>& values,
                                                              size_type tid) {
  static_assert(num_values > 0, "The number of values must be positive");

  hash_type seed = Hash(values[0][tid]);
  for (int i = 1; i < num_values; ++i) {
    seed = HashCombine(seed, values[i][tid]);
  }
  return seed;
}

template <int num_values, class T>
typename std::enable_if<num_values >= 6, hash_type>::type Hash(const Vector<const T*>& values,
                                                               size_type tid) {
  hash_type seed = Hash(values[0][tid]);
  for (int i = 1; i < static_cast<int>(values.size()); ++i) {
    seed = HashCombine(seed, values[i][tid]);
  }
  return seed;
}

template <int num_values, typename T>
typename std::enable_if<num_values < 6, bool>::type VectorEqualAt(
    const Vector<const T*>& left_values,
    const Vector<const T*>& right_values,
    size_type left_tid,
    size_type right_tid) {
  for (int i = 0; i < num_values; ++i) {
    if (left_values[i][left_tid] != right_values[i][right_tid]) {
      return false;
    }
  }
  return true;
}

template <int num_values, typename T>
typename std::enable_if<num_values >= 6, bool>::type VectorEqualAt(
    const Vector<const T*>& left_values,
    const Vector<const T*>& right_values,
    size_type left_tid,
    size_type right_tid) {
  for (int i = 0; i < static_cast<int>(left_values.size()); ++i) {
    if (left_values[i][left_tid] != right_values[i][right_tid]) {
      return false;
    }
  }
  return true;
}

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_HASH_HPP_ */
