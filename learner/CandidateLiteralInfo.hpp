/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_CANDIDATE_LITERAL_INFO_HPP_
#define QUICKFOIL_LEARNER_CANDIDATE_LITERAL_INFO_HPP_

#include "schema/TypeDefs.hpp"

namespace quickfoil {

class FoilLiteral;

struct CandidateLiteralInfo {
  // Takes ownership of <filter_predicate>.
  CandidateLiteralInfo(const FoilLiteral* literal_in)
      : literal(literal_in) {
  }

  const FoilLiteral* literal;
  size_type num_covered_positive = 0;
  size_type num_covered_negative = 0;
  size_type num_binding_positive = 0;
  size_type num_binding_negative = 0;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_CANDIDATE_LITERAL_INFO_HPP_ */
