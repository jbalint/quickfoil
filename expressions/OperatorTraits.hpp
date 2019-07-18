/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_EXPRESSIONS_OPERATOR_TRAITS_HPP_
#define QUICKFOIL_EXPRESSIONS_OPERATOR_TRAITS_HPP_

#include "expressions/ComparisonOperators.hpp"

namespace quickfoil {

enum OperatorType {
  kEqual = 0,
  kNotEqual,
  kLess,
  kLessOrEqual,
  kGreater,
  kGreaterOrEqual
};

template <OperatorType op>
struct OperatorTraits {};

template <>
struct OperatorTraits<OperatorType::kEqual>{
  typedef operators::Equal op;
};

template <>
struct OperatorTraits<OperatorType::kNotEqual>{
  typedef operators::NotEqual op;
};

template <>
struct OperatorTraits<OperatorType::kGreater>{
  typedef operators::Greater op;
};

template <>
struct OperatorTraits<OperatorType::kLess>{
  typedef operators::Less basic_operator;
};

template <>
struct OperatorTraits<OperatorType::kGreaterOrEqual>{
  typedef operators::GreaterOrEqual basic_operator;
};

template <>
struct OperatorTraits<OperatorType::kLessOrEqual>{
  typedef operators::LessOrEqual basic_operator;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_EXPRESSIONS_COMPARISON_OPERATOR_TRAITS_HPP_ */
