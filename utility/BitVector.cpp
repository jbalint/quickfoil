/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "utility/BitVector.hpp"

#include "dynamic_bitset/dynamic_bitset.hpp"

namespace quickfoil {

std::string BitVectorToString(const BitVector& bit_vec) {
  std::string bit_vec_str;
  third_party::boost::to_string(bit_vec, bit_vec_str);
  return bit_vec_str;
}

}  // namespace machine
