/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_MATH_HPP_
#define QUICKFOIL_UTILITY_MATH_HPP_

namespace quickfoil {

constexpr int GCD(int a, int b) {
  return (a == b) ? a : ((a > b) ? GCD(a-b, b) : GCD(a, b-a));
}

constexpr int LCM(int a, int b) {
  return a / GCD(a, b) * b;
}

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_MATH_HPP_ */
