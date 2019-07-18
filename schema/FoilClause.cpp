/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "schema/FoilClause.hpp"

#include "schema/FoilLiteral.hpp"
#include "schema/FoilVariable.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

bool FoilClause::Equals(const FoilClause& other) const {
  if (num_body_literals() != other.num_body_literals()) {
    return false;
  }
  if (head_literal_.Equals(other.head_literal_)) {
    return false;
  }
  for (int i = 0; i < num_body_literals(); ++i) {
    if (body_literals_[i].Equals(other.body_literals_[i])) {
      return false;
    }
  }
  return true;
}

std::string FoilClause::ToString() const {
  std::string ret;
  ret.append(head_literal_.ToString());
  ret.append(" :-  ");
  for (const FoilLiteral& body_literal : body_literals_) {
    ret.append(body_literal.ToString()).append(", ");
  }
  ret.erase(ret.end() - 2, ret.end());
  return ret;
}

void FoilClause::AddUnBoundBodyLiteral(const FoilLiteral& body_literal, bool is_random) {
  num_variables_without_last_body_literal_ = variables_.size();

  Vector<FoilVariable> literal_variables;
  literal_variables.reserve(body_literal.num_variables());
  for (const FoilVariable& variable : body_literal.variables()) {
    if (!variable.IsBound()) {
      literal_variables.emplace_back(variables_.size(),
                                     variable.variable_type_id());
      variables_.push_back(literal_variables.back());
    } else {
      DCHECK_LT(variable.variable_id(), static_cast<int>(variables_.size()));
      DCHECK(variables_[variable.variable_id()].IsBound());
      literal_variables.emplace_back(variable);
    }
  }
  body_literals_.emplace_back(body_literal.predicate(), literal_variables);
  random_flags_.emplace_back(is_random);
}

void FoilClause::AddBoundBodyLiteral(const FoilLiteral& body_literal,
                                     bool is_random) {
  num_variables_without_last_body_literal_ = variables_.size();

  DCHECK(body_literal.IsBound());
  for (const FoilVariable& variable : body_literal.variables()) {
    if (variable.variable_id() >= static_cast<int>(variables_.size())) {
      variables_.resize(variable.variable_id());
      variables_.emplace_back(variable);
    } else if (!variables_[variable.variable_id()].IsBound()) {
      variables_[variable.variable_id()] = variable;
    }
  }
  body_literals_.emplace_back(body_literal);
  random_flags_.emplace_back(is_random);
}

bool FoilClause::IsBound() const {
  for (const FoilLiteral& body_literal : body_literals_) {
    if (!body_literal.IsBound()) {
      return false;
    }
  }
  for (const FoilVariable& variable : variables_) {
    if (!variable.IsBound()) {
      return false;
    }
  }

  return true;
}

}  // namespace quickfoil

