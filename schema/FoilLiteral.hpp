/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_SCHEMA_FOILLITERAL_HPP_
#define QUICKFOIL_SCHEMA_FOILLITERAL_HPP_

#include <unordered_set>

#include "schema/FoilPredicate.hpp"
#include "schema/FoilVariable.hpp"
#include "utility/Hash.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class FoilLiteral {
 public:
  // Does not take ownership of <predicate>.
  FoilLiteral(const FoilPredicate* predicate)
      : predicate_(predicate) {
    variables_.reserve(predicate->num_arguments());
  }

  FoilLiteral(const FoilPredicate* predicate,
              const Vector<FoilVariable>& variables)
      : predicate_(predicate),
        variables_(variables) {
    DCHECK_EQ(predicate_->num_arguments(), static_cast<int>(variables_.size()));
    for (size_t i = 0; i < variables_.size(); ++i) {
      UpdateJoinKey(i);
    }
  }

  ~FoilLiteral() {}

  inline bool Equals(const FoilLiteral& other) const {
    if (predicate_ != other.predicate_) {
      return false;
    }
    DCHECK_EQ(num_variables(), other.num_variables());
    for (int i = 0; i < num_variables(); ++i) {
      if (!variable_at(i).Equals(other.variable_at(i))) {
        return false;
      }
    }
    return true;
  }

  const FoilPredicate* predicate() const {
    return predicate_;
  }

  const Vector<FoilVariable>& variables() const {
    return variables_;
  }

  const FoilVariable& variable_at(int index) const {
    DCHECK_LT(index, static_cast<int>(variables_.size()));
    return variables_[index];
  }

  void AddVariable(const FoilVariable& variable) {
    DCHECK_LT(num_variables(), predicate_->num_arguments());
    DCHECK_EQ(predicate_->argument_type_at(num_variables()), variable.variable_type_id());
    variables_.emplace_back(variable);
    UpdateJoinKey(variables_.size() - 1);
  }

  void AddVariable(FoilVariable&& variable) {
    DCHECK_LT(num_variables(), predicate_->num_arguments());
    DCHECK_EQ(predicate_->argument_type_at(num_variables()), variable.variable_type_id());
    variables_.push_back(std::move(variable));
    UpdateJoinKey(variables_.size() - 1);
  }

  inline int num_variables() const {
    return variables_.size();
  }

  inline int GetNumUnboundVariables() const {
    int num_unbound_vars = 0;
    for (const FoilVariable& variable : variables_) {
      if (!variable.IsBound()) {
        ++num_unbound_vars;
      }
    }
    return num_unbound_vars;
  }

  std::string ToString() const {
    DCHECK(!variables_.empty());
    std::string ret(predicate_->name());
    ret.push_back('(');
    ret.append(variables_[0].ToString());
    for (size_t i = 1; i < variables_.size(); ++i) {
      ret.append(", ").append(variables_[i].ToString());
    }
    ret.push_back(')');
    return ret;
  }

  bool IsBound() const {
    for (const FoilVariable& variable : variables_) {
      if (!variable.IsBound()) {
        return false;
      }
    }
    return true;
  }

  bool AreAllVariablesUnBound() const {
    for (const FoilVariable& variable : variables_) {
      if (variable.IsBound()) {
        return false;
      }
    }
    return true;
  }

  void Validate() const {
    CHECK_EQ(static_cast<int>(variables_.size()), predicate_->num_arguments());
    for (size_t i = 0; i < variables_.size(); ++i) {
      CHECK_EQ(predicate_->argument_type_at(i), variables_[i].variable_type_id())
          << ToString();
    }
  }

  inline void UpdateJoinKey(int pos) {
    if (variables_[pos].IsBound()) {
      if (join_key_ == -1
          || variables_[join_key_].variable_type_id() < variables_[pos].variable_type_id()) {
        join_key_ = pos;
      }
    }
  }

  inline int join_key() const {
    return join_key_;
  }

  FoilLiteral CreateUnBoundLiteral(const int start_unbound_var_id) const {
    FoilLiteral literal(predicate_);
    for (const FoilVariable& variable : variables_) {
      if (variable.variable_id() < start_unbound_var_id) {
        literal.AddVariable(variable);
      } else {
        literal.AddVariable(FoilVariable(variable.variable_type_id()));
      }
    }
    return literal;
  }

 private:
  const FoilPredicate* predicate_;
  Vector<FoilVariable> variables_;
  int join_key_ = -1;

  // Copybale.
};

struct FoilLiteralHash {
  inline size_t operator()(const FoilLiteral& literal) const {
    std::size_t seed = Hash(literal.predicate()->id());
    for (const FoilVariable& var : literal.variables()) {
      seed = HashCombine(seed, var.variable_id());
    }
    return seed;
  }
};

struct FoilLiteralEqual {
  inline bool operator()(const FoilLiteral& lhs, const FoilLiteral& rhs) const {
    return lhs.Equals(rhs);
  }
};

typedef std::unordered_set<FoilLiteral, FoilLiteralHash, FoilLiteralEqual> FoilLiteralSet;

} /* namespace quickfoil */

#endif /* QUICKFOIL_SCHEMA_FOILLITERAL_HPP_ */
