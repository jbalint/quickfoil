/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_QUICK_FOIL_STATE_HPP_
#define QUICKFOIL_LEARNER_QUICK_FOIL_STATE_HPP_

#include <memory>

#include "learner/LiteralSearchStats.hpp"
#include "learner/LiteralSelector.hpp"
#include "schema/FoilClause.hpp"
#include "storage/TableView.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

struct QuickFoilState {
  QuickFoilState(bool is_extended_from_tied_literal_in,
                 const FoilClauseConstSharedPtr& building_clause_in,
                 const std::shared_ptr<LiteralSearchStats>& literal_search_stats_in,
                 const FoilLiteralSet& black_random_literals_in,
                 const std::shared_ptr<TableView>& uncovered_positive_data_in)
      : is_extended_from_tied_literal(is_extended_from_tied_literal_in),
        building_clause(building_clause_in),
        literal_search_stats(literal_search_stats_in),
        black_random_literals(black_random_literals_in),
        uncovered_positive_data(uncovered_positive_data_in) {
    DCHECK(building_clause->random_flags().empty() || !building_clause->random_flags().back());
    DCHECK(uncovered_positive_data != nullptr);
  }

  QuickFoilState(bool is_extended_from_tied_literal_in,
                 const FoilClauseConstSharedPtr& building_clause_in,
                 const std::shared_ptr<QuickFoilState>& previous_state_in,
                 const std::shared_ptr<LiteralSearchStats>& literal_search_stats_in,
                 const FoilLiteralSet& black_random_literals_in,
                 const std::shared_ptr<TableView>& uncovered_positive_data_in)
      : is_extended_from_tied_literal(is_extended_from_tied_literal_in),
        building_clause(building_clause_in),
        previous_state(previous_state_in),
        literal_search_stats(literal_search_stats_in),
        black_random_literals(black_random_literals_in),
        uncovered_positive_data(uncovered_positive_data_in) {
    DCHECK(!building_clause->random_flags().empty() && building_clause->random_flags().back());
    DCHECK(uncovered_positive_data != nullptr);
  }

  bool is_extended_from_tied_literal;
  FoilClauseConstSharedPtr building_clause;
  std::shared_ptr<QuickFoilState> previous_state;
  std::unique_ptr<LiteralSelector> literal_selector;
  std::shared_ptr<LiteralSearchStats> literal_search_stats;
  FoilLiteralSet black_random_literals;
  std::shared_ptr<TableView> uncovered_positive_data;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_QUICKFOILSTATE_HPP_ */
