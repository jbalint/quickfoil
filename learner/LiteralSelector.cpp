/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "learner/LiteralSelector.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include "operations/BuildHashTable.hpp"
#include "operations/SemiJoin.hpp"
#include "operations/SemiJoinFactory.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/FoilHashTable.hpp"
#include "storage/TableView.hpp"
#include "utility/StringUtil.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace quickfoil {

DEFINE_int32(num_saved_literals,
             5,
             "Number of literals to be cached");

DEFINE_double(random_margin,
              0.03,
              "A literal with mcc score between (-random_margin, random_margin) is considered as a random literal");

LiteralSelector::LiteralSelector(const size_type total_uncovered_positive,
                                 const FoilClauseConstSharedPtr& clause,
                                 const FoilLiteralSet& black_random_literals)
    : total_uncovered_positive_(total_uncovered_positive),
      clause_(clause),
      clause_entropy_area_(ComputeAreaUnderEntropyCurve(clause_->GetNumPositiveBindings(),
                                                        clause_->GetNumNegativeBindings())),
      black_random_literals_(black_random_literals) {
  DVLOG(4) << "Black list: " << ContainerToString(black_random_literals_);
}

template <bool consider_random_literal>
void LiteralSelector::Insert(const CandidateLiteralInfo& literal_info) {
  const double raw_mcc = ComputeMccScore(literal_info.num_covered_positive,
                                         literal_info.num_covered_negative);
  const double mcc = 1 + raw_mcc;
  const double auec = 1 + ComputeEntropyScore(literal_info.num_binding_positive,
                                              literal_info.num_binding_negative);

  if (mcc == 0 || auec == 0) {
    DVLOG(4) << "Candidate literal " << literal_info.literal->ToString() << " is excluded, because "
             << "MCC or AUEC measure is 0 (mcc=" << mcc << " and auec=" << auec << "): "
             << "num_covered_positive=" << literal_info.num_covered_positive << ", "
             << "num_covered_negative=" << literal_info.num_covered_negative << ", "
             << "num_binding_positive=" << literal_info.num_binding_positive << ", "
             << "num_binding_negative=" << literal_info.num_binding_negative << ", "
             << "num_binding_positive_in_clause=" << clause_->GetNumPositiveBindings() << ", "
             << "num_binding_negative_in_clause=" << clause_->GetNumNegativeBindings() << ", "
             << "clause_precision=" << static_cast<double>(clause_->GetNumPositiveBindings())/clause_->GetNumTotalBindings();
    return;
  }

  const double score = 5 * auec * mcc / (mcc + 4 * auec);

  DVLOG(4) << "Candidate literal " << literal_info.literal->ToString() << ": "
           << "num_covered_positive=" << literal_info.num_covered_positive << ", "
           << "num_covered_negative=" << literal_info.num_covered_negative << ", "
           << "num_binding_positive=" << literal_info.num_binding_positive << ", "
           << "num_binding_negative=" << literal_info.num_binding_negative << ", "
           << "num_binding_positive_in_clause=" << clause_->GetNumPositiveBindings() << ", "
           << "num_binding_negative_in_clause=" << clause_->GetNumNegativeBindings() << ", "
           << "clause_precision=" << static_cast<double>(clause_->GetNumPositiveBindings())/clause_->GetNumTotalBindings() << ", "
           << "MCC score=" << ComputeMccScore(literal_info.num_covered_positive,
                                              literal_info.num_covered_negative) << ", "
           << "AUEC score=" << ComputeEntropyScore(literal_info.num_binding_positive,
                                                   literal_info.num_binding_negative) << ", "
           << "score=" << score;

  if (consider_random_literal) {
    if (!literal_info.literal->IsBound() &&
        !(raw_mcc == 0 && literal_info.num_binding_positive == literal_info.num_covered_positive &&
          literal_info.num_binding_negative == literal_info.num_covered_negative) &&
        raw_mcc < FLAGS_random_margin &&
        raw_mcc > -FLAGS_random_margin &&
        black_random_literals_.find(*literal_info.literal) == black_random_literals_.end() &&
        !NeedRegrow(*clause_,
                    literal_info.num_covered_negative,
                    *literal_info.literal)) {

      DVLOG(4) << "Random candidate literal " << literal_info.literal->ToString() << ": "
               << "mcc=" << mcc;

      const double precision = static_cast<double>(literal_info.num_covered_positive) / (literal_info.num_covered_positive + literal_info.num_covered_negative);
      const double recall = static_cast<double>(literal_info.num_covered_positive) / clause_->GetNumPositiveBindings();
      const double f_score = 2 * precision * recall / (precision + recall);

      if (best_random_literal_ == nullptr ||
          (f_score > maximum_random_f_score_ ||
           (f_score == maximum_random_f_score_ &&
            literal_info.num_covered_positive < total_uncovered_positive_ &&
            literal_info.num_binding_positive / literal_info.num_covered_positive <= 2 &&
            literal_info.num_binding_positive > best_random_literal_->candidate_literal_info.num_binding_positive))) {
        maximum_random_f_score_ = f_score;
        best_random_literal_.reset(new EvaluatedLiteralIntermediateInfo(literal_info, score));

        DVLOG(4) << "New best random candidate literal " << literal_info.literal->ToString() << ": "
                 << "f_score=" << f_score << ", regular_score="<<score;
      }
    }
  }

  if (static_cast<int>(top_literal_heap_.size()) < FLAGS_num_saved_literals) {
    top_literal_heap_.emplace_back(new EvaluatedLiteralIntermediateInfo(literal_info, score));
    std::push_heap(top_literal_heap_.begin(), top_literal_heap_.end(), min_heap_comparator_);

    DVLOG(5) << "Insert candidate literal into the heap " << literal_info.literal->ToString()
             << "(#heap size = " << top_literal_heap_.size() << ")";
  } else {
    if (score < top_literal_heap_.front()->score) {
      DVLOG(5) << "Ignore candidate literal " << literal_info.literal->ToString();
      return;
    }

    const double min_score = top_literal_heap_.front()->score;
    do {
      std::pop_heap(top_literal_heap_.begin(), top_literal_heap_.end(), min_heap_comparator_);

      DVLOG(6) << "Remove candidate literal " << top_literal_heap_.back()->candidate_literal_info.literal->ToString()
               << " (score=" << top_literal_heap_.back()->score << ")";

      delete top_literal_heap_.back();
      top_literal_heap_.pop_back();
    } while (!top_literal_heap_.empty() && top_literal_heap_.front()->score == min_score);

    top_literal_heap_.emplace_back(new EvaluatedLiteralIntermediateInfo(literal_info, score));
    std::push_heap(top_literal_heap_.begin(), top_literal_heap_.end(), min_heap_comparator_);

    DVLOG(5) << "Insert candidate literal into the heap " << literal_info.literal->ToString()
             << "(#heap size = " << top_literal_heap_.size() << ")";
  }
}

// TODO(qzeng): Record that the true positive coverage has been calculated so that it is not done
//              again when the literal is chosen as the last body literal of a new clause.
size_type LiteralSelector::ComputeCoveredPositives(const FoilLiteral& literal,
                                                   const std::shared_ptr<TableView>& uncovered_positive_data) {
  std::unique_ptr<TableView> positive_table;

  if (clause_->IsBindingDataConseuctive()) {
    positive_table.reset(new TableView(clause_->CreatePositiveBlocks()));
  } else {
    positive_table.reset(new TableView(clause_->positive_blocks()));
  }

  Vector<AttributeReference> background_join_keys;
  Vector<AttributeReference> clause_join_keys;
  const Vector<FoilVariable>& variables = literal.variables();
  for (int vid = 0; vid < static_cast<int>(variables.size()); ++vid) {
    if (variables[vid].IsBound()) {
      background_join_keys.emplace_back(vid);
      clause_join_keys.emplace_back(variables[vid].variable_id());
    }
  }

  Vector<int> project_column_ids;
  Vector<AttributeReference> coverage_join_keys;
  for (int i = 0; i < clause_->head_literal().num_variables(); ++i) {
    project_column_ids.emplace_back(i);
    coverage_join_keys.emplace_back(i);
  }

  std::unique_ptr<FoilHashTable> hash_table_for_bindings;
  std::unique_ptr<FoilHashTable> background_hash_table;
  std::unique_ptr<SemiJoin> binding_semijoin(
      SelectAndCreateSemiJoin(*positive_table,
                              literal.predicate()->fact_table(),
                              &hash_table_for_bindings,
                              &background_hash_table,
                              clause_join_keys,
                              background_join_keys,
                              project_column_ids));

  std::unique_ptr<FoilHashTable> positive_hash_table_for_coverage(
      BuildHashTableAfterSemiJoin(positive_table->num_tuples(),
                                  clause_->head_literal().num_variables(),
                                  binding_semijoin.release()));

  std::unique_ptr<SemiJoin> coverage_semijoin(
      CreateSemiJoin(true,
                     *uncovered_positive_data,
                     *positive_table,
                     *positive_hash_table_for_coverage,
                     coverage_join_keys,
                     coverage_join_keys,
                     project_column_ids));

  size_type num_covered_tuples = 0;
  std::unique_ptr<SemiJoinChunk> coverage_result(coverage_semijoin->Next());
  while (coverage_result != nullptr) {
    num_covered_tuples += coverage_result->num_ones;
    coverage_result.reset(coverage_semijoin->Next());
  }

  return num_covered_tuples;
}

bool LiteralSelector::ChooseRandomLiteral(
    const FoilLiteral& random_literal,
    const FoilLiteral& regular_literal,
    const std::shared_ptr<TableView>& uncovered_positive_data) {
  const size_type covered_positive_by_regular = ComputeCoveredPositives(
      regular_literal, uncovered_positive_data);
  const size_type origin_uncovered_data = uncovered_positive_data->num_tuples();

  QLOG << "Regular literal " << regular_literal.ToString() << ":  " << covered_positive_by_regular << " vs. " << origin_uncovered_data;
  if (covered_positive_by_regular >= 0.8 * origin_uncovered_data) {
    return false;
  }

  const size_type covered_positive_by_random = ComputeCoveredPositives(
      random_literal, uncovered_positive_data);
  QLOG << "Random literal " << random_literal.ToString() << ":  " << covered_positive_by_random << " vs. " << origin_uncovered_data;
  if (covered_positive_by_random < 1.2 * covered_positive_by_regular) {
    return false;
  }
  return true;
}

template void LiteralSelector::Insert<true>(const CandidateLiteralInfo& literal_info);
template void LiteralSelector::Insert<false>(const CandidateLiteralInfo& literal_info);

}  // namespace quickfoil
