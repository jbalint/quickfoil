/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_LITERAL_SELECTOR_HPP_
#define QUICKFOIL_LEARNER_LITERAL_SELECTOR_HPP_

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>

#include "learner/CandidateLiteralInfo.hpp"
#include "memory/MemoryUsage.hpp"
#include "schema/FoilClause.hpp"
#include "schema/TypeDefs.hpp"
#include "utility/ElementDeleter.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class TableView;

struct EvaluatedLiteralIntermediateInfo {
  EvaluatedLiteralIntermediateInfo(const CandidateLiteralInfo& candidate_literal_info_in,
                                   double score_in)
      : candidate_literal_info(candidate_literal_info_in),
        score(score_in) {}

  const CandidateLiteralInfo candidate_literal_info;
  double score;
};

// TODO(qzeng): This struct can now be replaced by EvaluatedLiteralIntermediateInfo. Maybe it should be removed.
struct EvaluatedLiteralInfo {
  EvaluatedLiteralInfo(const EvaluatedLiteralIntermediateInfo& itermediate_literal_info)
      : literal(*itermediate_literal_info.candidate_literal_info.literal),
        num_covered_positive(itermediate_literal_info.candidate_literal_info.num_covered_positive),
        num_covered_negative(itermediate_literal_info.candidate_literal_info.num_covered_negative),
        num_binding_positive(itermediate_literal_info.candidate_literal_info.num_binding_positive),
        num_binding_negative(itermediate_literal_info.candidate_literal_info.num_binding_negative),
        score(itermediate_literal_info.score) {
    DCHECK_LE(num_covered_positive, num_binding_positive);
    DCHECK_LE(num_covered_negative, num_binding_negative);
  }

  inline double GetPrecision() const {
    return static_cast<double>(num_binding_positive) / (num_binding_positive + num_binding_negative);
  }

  FoilLiteral literal;
  size_type num_covered_positive;
  size_type num_covered_negative;
  size_type num_binding_positive;
  size_type num_binding_negative;
  double score;
};

struct GreaterComparator {
  bool operator()(const EvaluatedLiteralIntermediateInfo*lhs, const EvaluatedLiteralIntermediateInfo* rhs) const {
    return lhs->score > rhs->score;
  }
};

struct LessComparator {
  bool operator()(const EvaluatedLiteralIntermediateInfo*lhs, const EvaluatedLiteralIntermediateInfo* rhs) const {
    return lhs->score < rhs->score;
  }
};

class LiteralSelector {
 public:
  LiteralSelector(const size_type total_uncovered_positive,
                  const FoilClauseConstSharedPtr& clause,
                  const FoilLiteralSet& black_random_literals);

  ~LiteralSelector() {
    DeleteElements(&top_literal_heap_);
    DeleteElements(&saved_literal_infos_);
  }

  template <bool consider_random_literal>
  void Insert(const CandidateLiteralInfo& literal_info);

  // The caller needs to take ownership of the pointers.
  bool GetBestLiteral(Vector<EvaluatedLiteralInfo*>* best_literals,
                      const std::shared_ptr<TableView>& uncovered_positive_data) {
    if (top_literal_heap_.empty()) {
      return false;
    }

    std::sort(top_literal_heap_.begin(), top_literal_heap_.end(), LessComparator());
    saved_literal_infos_.reserve(top_literal_heap_.size());

    int literal_idx;
    const double max_score = top_literal_heap_.back()->score;
    double random_score = -1;
    bool use_random_literal = false;
    if (best_random_literal_ != nullptr) {
      const double mcc = 2 + ComputeMccScore(best_random_literal_->candidate_literal_info.num_covered_positive,
                                             best_random_literal_->candidate_literal_info.num_covered_negative);
      const double auec = 1 + ComputeEntropyScore(best_random_literal_->candidate_literal_info.num_binding_positive,
                                                  best_random_literal_->candidate_literal_info.num_binding_negative);

      random_score = 5 * auec * mcc / (mcc + 4 * auec);
    }

    const EvaluatedLiteralIntermediateInfo* best_regular_literal = top_literal_heap_.back();
    if (random_score > max_score &&
        best_random_literal_->candidate_literal_info.num_covered_positive > best_regular_literal->candidate_literal_info.num_covered_positive &&
        (best_random_literal_->candidate_literal_info.num_binding_positive/best_random_literal_->candidate_literal_info.num_covered_positive < 50 ||
          static_cast<double>(best_regular_literal->candidate_literal_info.num_covered_positive)/clause_->GetNumPositiveBindings() < 0.1) &&
        NotExceedMemoryQuota(best_random_literal_->candidate_literal_info) &&
        ChooseRandomLiteral(*best_random_literal_->candidate_literal_info.literal,
                            *best_regular_literal->candidate_literal_info.literal,
                            uncovered_positive_data)) {
      literal_idx = top_literal_heap_.size() - 1;
      best_literals->emplace_back(new EvaluatedLiteralInfo(*best_random_literal_));
      use_random_literal = true;
    } else {
      for (literal_idx = top_literal_heap_.size() - 2; literal_idx >= 0; --literal_idx) {
        if (std::fabs(top_literal_heap_[literal_idx]->score - max_score) < 0.00001) {
          best_literals->emplace_back(new EvaluatedLiteralInfo(*top_literal_heap_[literal_idx]));
        } else {
          break;
        }
      }
      best_literals->emplace_back(new EvaluatedLiteralInfo(*best_regular_literal));
    }

    for (int i = 0; i <= literal_idx; ++i) {
      DVLOG(4) << "Saved literals: " << top_literal_heap_[i]->candidate_literal_info.literal->ToString() << ", "
               << "score: " << top_literal_heap_[i]->score;
      saved_literal_infos_.emplace_back(new EvaluatedLiteralInfo(*top_literal_heap_[i]));
    }

    return use_random_literal;
  }

  void GetNextBestLiterals(Vector<EvaluatedLiteralInfo*>* best_literals) {
    if (Empty()) {
      return;
    }

    const double max_score = saved_literal_infos_.back()->score;
    do {
      best_literals->emplace_back(saved_literal_infos_.back());
      saved_literal_infos_.pop_back();
    } while (!Empty() && std::fabs(saved_literal_infos_.back()->score - max_score) < 0.00001);
  }

  inline bool Empty() const {
    return saved_literal_infos_.empty();
  }

  inline static bool NeedRegrow(const FoilClause& clause,
                                const EvaluatedLiteralInfo& best_literal_info) {
    return NeedRegrow(clause,
                      best_literal_info.num_covered_negative,
                      best_literal_info.literal);
  }

 private:
  bool ChooseRandomLiteral(const FoilLiteral& random_literal,
                           const FoilLiteral& regular_literal,
                           const std::shared_ptr<TableView>& uncovered_positive_data);

  size_type ComputeCoveredPositives(const FoilLiteral& literal,
                                    const std::shared_ptr<TableView>& uncovered_positive_data);

  inline static bool NeedRegrow(const FoilClause& clause,
                                const size_type num_covered_negative,
                                const FoilLiteral& literal) {
    if (clause.body_literals().empty() || num_covered_negative == 0) {
      return false;
    }

    if (!clause.random_flags().back()) {
      return false;
    }

    for (int i = 0; i < literal.num_variables(); ++i) {
      if (literal.variable_at(i).variable_id() >=
          clause.num_variables_without_last_body_literal() &&
          i != literal.predicate()->key()) {
        return false;
      }
    }

    return true;
  }

  inline bool NotExceedMemoryQuota(const CandidateLiteralInfo& literal_info) const {
#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
    // Times 3 to count the hash tables and partitions.
    const std::size_t memory_usage_for_new_binding_set =
        (static_cast<std::size_t>(literal_info.literal->GetNumUnboundVariables() + clause_->num_variables()) *
         (literal_info.num_binding_positive + literal_info.num_binding_negative) *
         TypeTraits<kQuickFoilDefaultDataType>::size) * 3;
    QLOG << "Current memory usage is " << MemoryUsage::GetInstance()->GetMemoryUsageInGB() << "GB, "
         << " the new binding set requires " << memory_usage_for_new_binding_set/(1024.0 * 1024 * 1024) << "GB";
    return MemoryUsage::GetInstance()->NotExceedQuotaWithNewAllocation(memory_usage_for_new_binding_set);
#else
    return true;
#endif
  }

  double ComputeMccScore(size_type num_covered_positive, size_type num_covered_negative) const;
  static double ComputeAreaUnderEntropyCurve(size_type num_positive_bindings,
                                             size_type num_negative_bindings);
  double ComputeEntropyScore(size_type num_positive_bindings,
                             size_type num_negative_bindings) const;

  Vector<EvaluatedLiteralIntermediateInfo*> top_literal_heap_;  // Owned the pointers.
  GreaterComparator min_heap_comparator_;

  Vector<EvaluatedLiteralInfo*> saved_literal_infos_;

  const size_type total_uncovered_positive_;
  FoilClauseConstSharedPtr clause_;
  const double clause_entropy_area_;
  const FoilLiteralSet& black_random_literals_;

//  double maximum_random_mcc_ = -1;
  double maximum_random_f_score_ = -1;
  std::unique_ptr<EvaluatedLiteralIntermediateInfo> best_random_literal_;

  DISALLOW_COPY_AND_ASSIGN(LiteralSelector);
};


inline double LiteralSelector::ComputeMccScore(
    size_type num_covered_positive,
    size_type num_covered_negative) const {
  if (num_covered_positive == 0) {
    // This deviates from the actual MCC.
    return -1;
  }
  if (num_covered_positive == clause_->GetNumPositiveBindings() &&
      num_covered_negative == clause_->GetNumNegativeBindings()) {
    return 0;
  }

  const double true_negatives = clause_->GetNumNegativeBindings() - num_covered_negative;
  const double false_negatives = clause_->GetNumPositiveBindings() - num_covered_positive;
  const double num_total_covered = num_covered_positive + num_covered_negative;

  return (num_covered_positive * true_negatives -
      num_covered_negative * false_negatives) /
      std::sqrt(num_total_covered *
       (clause_->GetNumTotalBindings() - num_total_covered) *
       clause_->GetNumNegativeBindings() *
       clause_->GetNumPositiveBindings());
}

inline double LiteralSelector::ComputeEntropyScore(
    size_type num_positive_bindings,
    size_type num_negative_bindings) const {
  return ComputeAreaUnderEntropyCurve(num_positive_bindings, num_negative_bindings) -
      clause_entropy_area_;
}

inline double LiteralSelector::ComputeAreaUnderEntropyCurve(
    size_type num_positive_bindings,
    size_type num_negative_bindings) {
  if (num_positive_bindings == 0) {
    return 0;
  }

  if (num_negative_bindings == 0) {
    return 1;
  }

  double precision = static_cast<double>(num_positive_bindings) / (num_positive_bindings + num_negative_bindings);
  return ((1 - precision) * (1 - precision) * std::log2(1 - precision) -
      precision * precision * std::log2(precision)) * std::log(2) + precision;
}

extern template void LiteralSelector::Insert<true>(const CandidateLiteralInfo& literal_info);
extern template void LiteralSelector::Insert<false>(const CandidateLiteralInfo& literal_info);

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_LITERALSELECTOR_HPP_ */
