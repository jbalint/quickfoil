/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_QUICK_FOIL_HPP_
#define QUICKFOIL_LEARNER_QUICK_FOIL_HPP_

#include <memory>

#include "learner/CandidateLiteralEnumerator.hpp"
#include "learner/QuickFoilState.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "schema/FoilClause.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#ifndef NDEBUG
#ifndef ENABLE_TIMING
#define ENABLE_TIMING
#endif
#endif

namespace quickfoil {

class LiteralSelector;
struct EvaluatedLiteralInfo;
class FoilLiteral;
class FoilPredicate;
class SemiJoinChunk;

class QuickFoil {
 public:
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  // Takes ownership of any pointers.
  QuickFoil(const size_type num_true_facts,
            const size_type num_false_facts,
            const FoilPredicate* target_predicate,
            const Vector<const FoilPredicate*>& background_predicates);

  ~QuickFoil();

  void Learn();

  const FoilPredicate* target_predicate() const {
    return target_predicate_;
  }

  const Vector<const FoilPredicate*>& background_predicates() const {
    return background_predicates_;
  }

  const Vector<std::unique_ptr<const FoilClause>>& learnt_clauses() const {
    return learnt_clauses_;
  }

 private:
  struct TiedLiteralInfo;

  template <bool consider_random_literal>
  void EvaluateAllCandidateLiterals(
      const Vector<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>>& predicate_literal_info_groups,
      LiteralSelector* selector,
      std::unordered_set<const FoilLiteral*>* pruned_literals_by_covered_results);

//  void CreateInitialPositiveAndNegativeTableViews();
//
//  void CreateInitialBuildingClause(const size_type num_true_facts,
//                                   const size_type num_false_facts);

  void CreateMostGeneralBuildingClause();

  void CreateLiteralEvaluationInfoGroups(
      const std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>& candidate_literals,
      Vector<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>>* predicate_literal_info_groups) const;

  bool ContinueRuleSearch() const;

  FoilClauseConstSharedPtr AddLiteralToBuildingClause(const EvaluatedLiteralInfo* literal_info, bool is_random);

  bool AddBuildingClauseWithNewLiteral(const EvaluatedLiteralInfo* literal_info_in);

  void ComputeCoverageOnUncoveredData(const FoilLiteral& literal,
                                      std::unique_ptr<FoilHashTable>* positive_hash_table_for_coverage,
                                      size_type* num_positives_covered,
                                      size_type* num_negatives_covered,
                                      Vector<SemiJoinChunk*>* positive_coverage_results);

  bool ShouldConsiderAsLastLiteral(const EvaluatedLiteralInfo& best_literal_info);

  bool AddBestCandidateLiteral(const bool is_tied_literal,
                               const bool is_random_literal,
                               const EvaluatedLiteralInfo* best_literal_info_in,
                               const std::shared_ptr<LiteralSearchStats>& literal_search_stats,
                               std::unique_ptr<LiteralSelector>* selector);

  const FoilPredicate* target_predicate_;
  Vector<const FoilPredicate*> background_predicates_;

  std::shared_ptr<QuickFoilState> building_state_;
  Vector<std::unique_ptr<const FoilClause>> learnt_clauses_;

  std::shared_ptr<TableView> global_uncovered_positive_data_;
  std::unique_ptr<TableView> original_negative_data_;

  const size_type maximum_uncovered_positive_;
  int current_outer_iterations_ = 0;

  std::shared_ptr<LiteralSearchStats> literal_serarch_stats_for_first_iteration_;

  CandidateLiteralEnumerator candidate_literal_enumerator_;

  // Owns the pointer.
  Vector<std::unique_ptr<TiedLiteralInfo>> tied_literal_infos_;

  DISALLOW_COPY_AND_ASSIGN(QuickFoil);
};

} /* namespace quickfoil */

#endif /* QUICKFOIL_LEARNER_QUICK_FOIL_HPP_ */
