/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_COUNTAGGREGATOR_HPP_
#define QUICKFOIL_OPERATIONS_COUNTAGGREGATOR_HPP_

#include <memory>

#include "learner/PredicateEvaluationPlan.hpp"
#include "operations/Filter.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class CountAggregator {
 public:
  CountAggregator(Filter* filter,
                  Vector<Vector<PredicateEvaluationPlan>>&& score_plans)
      : filter_(filter),
        score_plans_(std::move(score_plans)) {}

  void Execute(const size_type num_positive);

  void ExecuteOnPositives();

  void ExecuteOnNegatives();

 private:
  template <bool positive>
  void ExecuteOnOneLabel();

  void LabelBitVector(const size_type num_positive,
                      const Vector<size_type>& build_tids,
                      BitVector* bit_vector) const;

  void UpdateSemiBitVector(const Vector<size_type>& build_relative_tids,
                           const BitVector& join_bitvector,
                           const std::size_t num_ones,
                           size_type* count,
                           BitVector* semi_bitvector) const;

  void UpdateSemiBitVectorWithNoFilter(const Vector<size_type>& build_relative_tids,
                                       size_type* count,
                                       BitVector* semi_bitvector) const;

  std::unique_ptr<Filter> filter_;
  Vector<Vector<PredicateEvaluationPlan>> score_plans_;

  DISALLOW_COPY_AND_ASSIGN(CountAggregator);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_COUNTAGGREGATOR_HPP_ */
