/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_BIT_VECTOR_HPP_
#define QUICKFOIL_UTILITY_BIT_VECTOR_HPP_

#include "dynamic_bitset/dynamic_bitset.hpp"

namespace quickfoil {
namespace third_party {
namespace boost {

template <typename Block = unsigned long,
          typename Allocator = std::allocator<Block> >
class dynamic_bitset;

}  // namespace third_party
}  // namespace boost
}  // namespace quickfoil

namespace quickfoil {

typedef quickfoil::third_party::boost::dynamic_bitset<>::allocator_type BitVectorAllocator;
typedef quickfoil::third_party::boost::dynamic_bitset<> BitVector;

std::string BitVectorToString(const BitVector& bit_vec);

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_BIT_VECTOR_HPP_ */
