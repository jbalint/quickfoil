/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_VECTOR_HPP_
#define QUICKFOIL_UTILITY_VECTOR_HPP_

#include "folly/FBVector.h"

namespace quickfoil {

template <class T, class Allocator = std::allocator<T>>
using Vector = folly::fbvector<T, Allocator>;

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_VECTOR_HPP_ */
