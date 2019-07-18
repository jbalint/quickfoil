/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_CANDIDATE_LITERAL_EVALUATOR_HPP_
#define QUICKFOIL_LEARNER_CANDIDATE_LITERAL_EVALUATOR_HPP_

#include <unordered_map>

#include "expressions/ComparisonPredicate.hpp"
#include "learner/PredicateEvaluationPlan.hpp"
#include "schema/FoilClause.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class CandidateLiteralInfo;
class FoilLiteral;
class PredicateEvaluationPlan;

class CandidateLiteralEvaluator {
 public:
  CandidateLiteralEvaluator(const FoilClauseConstSharedPtr& building_clause)
      : building_clause_(building_clause) {}

  void Evaluate(int clause_join_key_id,
                const std::unordered_map<const FoilPredicate*,
                                         Vector<const FoilLiteral*>>& literal_groups,
                Vector<CandidateLiteralInfo*>* results);

 private:
  void GeneratePredicateEvaluationPlan(const Vector<CandidateLiteralInfo*>& literals,
                                       Vector<FoilFilterPredicate>* predicates,
                                       PredicateEvaluationPlan* literal_evalution_plan);

  std::string OutputPredicateEvaluationPlan(const Vector<CandidateLiteralInfo*>& literals,
                                            const Vector<FoilFilterPredicate>& predicates,
                                            const PredicateEvaluationPlan& literal_evaluation_plan);

  const FoilClauseConstSharedPtr& building_clause_;

  DISALLOW_COPY_AND_ASSIGN(CandidateLiteralEvaluator);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_CANDIDATE_LITERAL_EVALUATOR_HPP_ */
