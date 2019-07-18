/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_LITERAL_SEARCH_STATS_HPP_
#define QUICKFOIL_LEARNER_LITERAL_SEARCH_STATS_HPP_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "schema/FoilLiteral.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class FoilPredicate;

struct LiteralSearchStats {
  // Takes ownership of pruned_literals_by_covered_results_in.
  LiteralSearchStats(std::shared_ptr<std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>>& generated_candidate_literals_in,
                     std::unordered_set<const FoilLiteral*>* pruned_literals_by_covered_results_in)
      : generated_candidate_literals(generated_candidate_literals_in),
        pruned_literals_by_covered_results(pruned_literals_by_covered_results_in) {
  }

  LiteralSearchStats() {}

  std::shared_ptr<std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>> generated_candidate_literals;
  std::unique_ptr<std::unordered_set<const FoilLiteral*>> pruned_literals_by_covered_results;  // Owned in generated_candidate_literals.
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_LITERAL_SEARCH_STATS_HPP_ */
