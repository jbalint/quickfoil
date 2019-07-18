/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_EXPRESSIONS_COMPARISON_OPERATORS_HPP_
#define QUICKFOIL_EXPRESSIONS_COMPARISON_OPERATORS_HPP_

namespace quickfoil {
namespace operators {

struct Equal {
  template<typename T1, typename T2>
  inline bool operator()(const T1& a, const T2& b) const { return a == b; }
};

struct NotEqual {
  template<typename T1, typename T2>
  inline bool operator()(const T1& a, const T2& b) const { return a != b; }
};

struct Less {
  template<typename T1, typename T2>
  inline  bool operator()(const T1& a, const T2& b) const { return a < b; }
};

struct Greater {
  template<typename T1, typename T2>
  inline bool operator()(const T1& a, const T2& b) const { return a > b; }
};

struct LessOrEqual {
  template<typename T1, typename T2>
  inline bool operator()(const T1& a, const T2& b) const { return !(a > b); }
};

struct GreaterOrEqual {
  template<typename T1, typename T2>
  bool operator()(const T1& a, const T2& b) const { return !(a < b); }
};

}  // namespace operators
}  // namespace quickfoil

#endif /* QUICKFOIL_EXPRESSIONS_COMPARISONOPERATORS_HPP_ */
