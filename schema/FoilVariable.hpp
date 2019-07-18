/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_SCHEMA_FOIL_VARIABLE_HPP_
#define QUICKFOIL_SCHEMA_FOIL_VARIABLE_HPP_

#include <string>
#include <unordered_set>

#include "utility/Macros.hpp"

namespace quickfoil {

class FoilVariable {
 public:
  FoilVariable()
      : variable_id_(-1), variable_type_id_(-1) {}

  // Unbound variable.
  FoilVariable(int variable_type_id)
      : variable_id_(-1), variable_type_id_(variable_type_id) {}

  FoilVariable(int variable_id, int variable_type_id)
      : variable_id_(variable_id), variable_type_id_(variable_type_id) {}

  inline bool Equals(const FoilVariable& other) const {
    return variable_id_ == other.variable_id_;
  }

  int variable_id() const { return variable_id_; }

  int variable_type_id() const { return variable_type_id_; }

  bool IsBound() const {
    return variable_id_ >= 0;
  }

  std::string ToString() const {
    return std::to_string(variable_id_);
  }

 private:
  // -1 if not bound.
  int variable_id_;
  int variable_type_id_;

  // Copyable.
};

struct FoilVariableHash {
  inline std::size_t operator()(const FoilVariable& var) const {
    return var.variable_id();
  }
};

struct FoilVariableEqual {
  inline bool operator()(const FoilVariable& lhs, const FoilVariable& rhs) const {
    return lhs.Equals(rhs);
  }
};

typedef std::unordered_set<FoilVariable, FoilVariableHash, FoilVariableEqual> FoilVariableSet;

}  // namespace quickfoil

#endif /* QUICKFOIL_SCHEMA_FOIL_VARIABLE_HPP_ */
