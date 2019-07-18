/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_CANDIDATE_LITERALE_NUMERATOR_HPP_
#define QUICKFOIL_LEARNER_CANDIDATE_LITERALE_NUMERATOR_HPP_

#include <unordered_map>
#include <unordered_set>

#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilVariable.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class CandidateLiteralEnumeratorTest;
class CandidateLiteralInfo;
class FoilPredicate;
struct LiteralSearchStats;

class CandidateLiteralEnumerator {
 public:
  CandidateLiteralEnumerator(const Vector<const FoilPredicate*>& background_predicates)
      : background_predicates_(background_predicates) {}

  ~CandidateLiteralEnumerator() {}

  void EnumerateCandidateLiterals(
      const FoilClause& building_clause,
      const LiteralSearchStats& last_run_stats,
      std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals);

 private:
  friend class CandidateLiteralEnumeratorTest;

  void EnumerateForMostGeneralClause(
      const FoilClause& building_clause,
      const LiteralSearchStats& last_run_stats,
      std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals);

  void EnumerateForMostGeneralClauseFromScratch(
      const FoilClause& building_clause,
      std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals);

  void EnumerateForNonMostGeneralClause(
      const FoilClause& building_clause,
      const LiteralSearchStats& last_run_stats,
      std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals);

  template <bool has_key>
  void GenerateAndPruneCandidateLiteralForPredicate(
      const FoilClause& building_clause,
      const LiteralSearchStats& last_run_stats,
      const FoilPredicate* background_predicate,
      const std::unordered_map<int, Vector<FoilVariable>>& variable_type_to_variable_map,
      const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map,
      std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals);

  void PruneLiteralsByKey(const FoilPredicate& background_predicate,
                          const Vector<const FoilLiteral*>& body_literals,
                          Vector<FoilLiteral>* candidate_literals);

  bool CheckReplaceableDuplicate(
      const FoilClause& clause,
      const FoilLiteral& literal,
      const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map);

  void GenerateCandidateLiterals(
      int num_arguments_filled,
      const Vector<Vector<FoilVariable>>& variables_per_argument,
      Vector<FoilLiteral>* candidate_literals) const;

  template <bool hash_key>
  void GenerateVariableVectorForPredicate(
      const std::unordered_map<int, Vector<FoilVariable>>& variable_type_to_variable_map,
      const FoilPredicate* predicate,
      const FoilVariableSet& key_variables,
      Vector<Vector<FoilVariable>>* variables_per_argument) const;

  void GenerateCandidateLiteralsFromAnother(
      const FoilLiteral& base_literal,
      int num_arguments_filled,
      const Vector<Vector<FoilVariable>>& variables_per_argument,
      Vector<FoilLiteral>* candidate_literals) const;

  std::unordered_map<int, Vector<Vector<int>>>::const_iterator CreateCanonicalDatabase(
      const FoilClause& clause,
      const FoilPredicate& predicate,
      const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map);

  void AddRowToCanonicalRelationForExistingLiteral(
      const FoilLiteral& literal,
      Vector<Vector<int>>* canonical_rel) const;

  void AddRowToCanonicalRelationForNewLiteral(
      const FoilLiteral& literal,
      int new_variable_start_id,
      Vector<Vector<int>>* canonical_rel) const;

  template <bool check_bound_variable, bool return_if_any>
  bool NestedLoopsJoin(const FoilLiteral& literal,
                       const Vector<Vector<int>>& canonical_predicate_rel,
                       Vector<Vector<int>>* canonical_joined_rel) const;

  // The joined result on the canonical database of the clause after the literals
  // of one predicate are all removed.
  std::unordered_map<int, Vector<Vector<int>>> canonical_databases_;  // row-wise.

  const Vector<const FoilPredicate*>& background_predicates_;

  DISALLOW_COPY_AND_ASSIGN(CandidateLiteralEnumerator);
};

inline void CandidateLiteralEnumerator::EnumerateCandidateLiterals(
    const FoilClause& building_clause,
    const LiteralSearchStats& last_run_stats,
    std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
    std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals) {
  if (building_clause.body_literals().empty()) {
    EnumerateForMostGeneralClause(building_clause, last_run_stats, entire_generated_literals, pruned_generated_literals);
  } else {
    EnumerateForNonMostGeneralClause(building_clause, last_run_stats, entire_generated_literals, pruned_generated_literals);
  }
}

} /* namespace quickfoil */

#endif /* QUICKFOIL_LEARNER_CANDIDATE_LITERALE_NUMERATOR_HPP_ */
