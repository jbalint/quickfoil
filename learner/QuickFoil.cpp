/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "learner/QuickFoil.hpp"

#include <memory>
#include <utility>

#include "expressions/AttributeReference.hpp"
#include "learner/CandidateLiteralEnumerator.hpp"
#include "learner/CandidateLiteralEvaluator.hpp"
#include "learner/CandidateLiteralInfo.hpp"
#include "learner/LiteralSearchStats.hpp"
#include "learner/LiteralSelector.hpp"
#include "memory/Buffer.hpp"
#include "memory/MemoryUsage.hpp"
#include "operations/BuildHashTable.hpp"
#include "operations/MultiColumnHashJoin.hpp"
#include "operations/SemiJoin.hpp"
#include "operations/SemiJoinFactory.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilPredicate.hpp"
#include "schema/FoilVariable.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/ElementDeleter.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace quickfoil {

DEFINE_double(positive_threshold,
              0.8,
              "The minimum number of covered examples for stopping the learning process");

DEFINE_int32(max_iterations,
             1000,
             "The maximum number of iterations to run");

DEFINE_double(minimum_inflated_precision,
              0.85,
              "The minimum precision calculated on bindings for a learnt clause");

DEFINE_double(minimum_true_precision,
              0.8,
              "The minimum precision calculated on examples for a learnt clause");

DEFINE_double(minimum_f_score,
              0.85,
              "The minimum F score calculated on examples for a learnt clause");

DEFINE_int32(maximum_clause_length,
             25,
             "The maximum number of literals in one learnt clause");

DEFINE_int32(maximum_random_literals,
             2,
             "The maximum number of random literals in one learnt clause");

DEFINE_int32(maximum_random_trials,
             5,
             "The maximum number of failed random literals for a rule search iteration");

DEFINE_double(minimum_coverage_for_tied_literal,
              0.1,
              "The minimum ratio of currently covered bindings to the uncovered examples"
              "for a saved tied literal");

struct QuickFoil::TiedLiteralInfo {
  TiedLiteralInfo(const EvaluatedLiteralInfo* literal_info_in,
                  const std::shared_ptr<QuickFoilState>& building_state_in,
                  const std::shared_ptr<LiteralSearchStats>& literal_search_stats_in)
      : literal_info(literal_info_in),
        building_state(building_state_in),
        literal_search_stats(literal_search_stats_in) {}

  std::unique_ptr<const EvaluatedLiteralInfo> literal_info;
  std::shared_ptr<QuickFoilState> building_state;
  std::shared_ptr<LiteralSearchStats> literal_search_stats;
};

QuickFoil::QuickFoil(const size_type num_true_facts,
                     const size_type num_false_facts,
                     const FoilPredicate* target_predicate,
                     const Vector<const FoilPredicate*>& background_predicates)
    : target_predicate_(target_predicate),
      background_predicates_(background_predicates),
      maximum_uncovered_positive_(num_true_facts * (1 - FLAGS_positive_threshold)),
      literal_serarch_stats_for_first_iteration_(new LiteralSearchStats),
      candidate_literal_enumerator_(background_predicates_) {
  CHECK_GT(num_false_facts, 0) << "Positive-only data is not supported";

  FoilLiteral head_literal(target_predicate_);
  for (int i = 0; i < target_predicate_->num_arguments(); ++i) {
    head_literal.AddVariable(std::move(FoilVariable(i, target_predicate_->argument_type_at(i))));
  }

  const FoilClauseConstSharedPtr initial_clause(FoilClause::Create(head_literal,
                                                                   num_true_facts,
                                                                   num_false_facts,
                                                                   target_predicate_->fact_table().columns()));

  global_uncovered_positive_data_.reset(new TableView(initial_clause->CreatePositiveBlocks()));
  original_negative_data_.reset(new TableView(initial_clause->CreateNegativeBlocks()));

  building_state_ = std::make_shared<QuickFoilState>(false,
                                                     initial_clause,
                                                     literal_serarch_stats_for_first_iteration_,
                                                     FoilLiteralSet(),
                                                     global_uncovered_positive_data_);
}

QuickFoil::~QuickFoil() {
}

inline bool QuickFoil::ContinueRuleSearch() const {
  return global_uncovered_positive_data_->num_tuples() > maximum_uncovered_positive_ &&
      current_outer_iterations_ < FLAGS_max_iterations;
}

void QuickFoil::CreateMostGeneralBuildingClause() {
  FoilLiteral head_literal(target_predicate_);
  for (int i = 0; i < target_predicate_->num_arguments(); ++i ) {
    head_literal.AddVariable(std::move(FoilVariable(i, target_predicate_->argument_type_at(i))));
  }

  building_state_ = std::make_shared<QuickFoilState>(false,
                                                     FoilClause::Create(head_literal,
                                                                        global_uncovered_positive_data_->columns(),
                                                                        original_negative_data_->columns()),
                                                     literal_serarch_stats_for_first_iteration_,
                                                     FoilLiteralSet(),
                                                     global_uncovered_positive_data_);
}

void QuickFoil::CreateLiteralEvaluationInfoGroups(
    const std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>& candidate_literals,
    Vector<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>>* predicate_literal_info_groups) const {
  for (const auto& predicate_and_candidate_literals : candidate_literals) {
    const FoilPredicate* predicate = predicate_and_candidate_literals.first;
    for (const FoilLiteral* candidate_literal : predicate_and_candidate_literals.second) {
      const int join_key = candidate_literal->join_key();
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>& literal_group_map =
          predicate_literal_info_groups->at(candidate_literal->variable_at(join_key).variable_id());
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>::iterator literal_group_it =
          literal_group_map.find(predicate);
      if (literal_group_it == literal_group_map.end()) {
        literal_group_it = literal_group_map.emplace(std::piecewise_construct,
                                                     std::forward_as_tuple(predicate),
                                                     std::forward_as_tuple()).first;
      }
      literal_group_it->second.emplace_back(candidate_literal);
    }
  }
}

inline bool QuickFoil::ShouldConsiderAsLastLiteral(const EvaluatedLiteralInfo& best_literal_info) {
  if (best_literal_info.GetPrecision() < FLAGS_minimum_inflated_precision &&
      building_state_->building_clause->num_body_literals() < FLAGS_maximum_clause_length) {
    return false;
  }
  return true;
}

void QuickFoil::Learn() {
  for (;;) {
    QLOG << "Rule search iteration: " << current_outer_iterations_
         << "(#global uncovered positive=" << global_uncovered_positive_data_->num_tuples() <<", "
         << "#local uncovered positive=" << building_state_->uncovered_positive_data->num_tuples();
#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
    QLOG << "Memory usage: " << MemoryUsage::GetInstance()->GetMemoryUsageInGB() << "GB";
#endif

    for (;;) {
      QLOG << "Literal search iteration: " << building_state_->building_clause->num_body_literals() << "\n"
           << "Building clause: " << building_state_->building_clause->ToString() << "\n"
           << "Num positive/negative bindings: " << building_state_->building_clause->GetNumPositiveBindings()
           << "/" << building_state_->building_clause->GetNumNegativeBindings();
#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
    QLOG << "Memory usage: " << MemoryUsage::GetInstance()->GetMemoryUsageInGB() << "GB";
#endif

      START_TIMER(QuickFoilTimer::kGenerateCandidateLiterals);

      std::shared_ptr<std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>> entire_generated_literals(
          new std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>);
      std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>> pruned_generated_literals;
      candidate_literal_enumerator_.EnumerateCandidateLiterals(*building_state_->building_clause,
                                                               *building_state_->literal_search_stats,
                                                               entire_generated_literals.get(),
                                                               &pruned_generated_literals);

      STOP_TIMER(QuickFoilTimer::kGenerateCandidateLiterals);
      START_TIMER(QuickFoilTimer::kGroupLiterals);

      const size_type local_num_uncovered_positives = building_state_->uncovered_positive_data->num_tuples();
      std::unique_ptr<std::unordered_set<const FoilLiteral*>> pruned_literals_by_covered_results(new std::unordered_set<const FoilLiteral*>);
      std::unique_ptr<LiteralSelector> selector(new LiteralSelector(local_num_uncovered_positives,
                                                                    building_state_->building_clause,
                                                                    building_state_->black_random_literals));

      Vector<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>> predicate_literal_info_groups(
          building_state_->building_clause->num_variables());
      CreateLiteralEvaluationInfoGroups(pruned_generated_literals,
                                        &predicate_literal_info_groups);

      STOP_TIMER(QuickFoilTimer::kGroupLiterals);

      if (building_state_->building_clause->GetNumRandomLiterals() < FLAGS_maximum_random_literals &&
          static_cast<int>(building_state_->black_random_literals.size()) < FLAGS_maximum_random_trials &&
          building_state_->building_clause->GetNumPositiveBindings() / local_num_uncovered_positives < 50) {
        EvaluateAllCandidateLiterals<true>(predicate_literal_info_groups,
                                           selector.get(),
                                           pruned_literals_by_covered_results.get());
      } else {
        EvaluateAllCandidateLiterals<false>(predicate_literal_info_groups,
                                            selector.get(),
                                            pruned_literals_by_covered_results.get());
      }

      std::shared_ptr<LiteralSearchStats> literal_search_stats =
          std::make_shared<LiteralSearchStats>(entire_generated_literals,
                                               pruned_literals_by_covered_results.release());

      Vector<EvaluatedLiteralInfo*> best_literal_info_vec;
      ElementDeleter<EvaluatedLiteralInfo> best_literal_info_vec_deleter(&best_literal_info_vec);
      bool is_random_literal = selector->GetBestLiteral(&best_literal_info_vec,
                                                        building_state_->uncovered_positive_data);

      if (best_literal_info_vec.empty()) {
        DLOG(ERROR) << "No valid candidate literal is found";
        CHECK(!tied_literal_infos_.empty());
        break;
      }

      std::unique_ptr<const EvaluatedLiteralInfo> best_literal_to_add(best_literal_info_vec.back());
      best_literal_info_vec.pop_back();
      DCHECK_GT(best_literal_to_add->num_binding_positive, 0);

      if (LiteralSelector::NeedRegrow(*building_state_->building_clause,
                                      *best_literal_to_add)) {
        QLOG << "The literal " << best_literal_to_add->literal.ToString()
             << " does not reference the last random literal in the building clause "
             << building_state_->building_clause->ToString()
             << ", and we need to choose another literal";

        is_random_literal = false;
        LiteralSelector* local_selector = selector.get();
        best_literal_to_add.reset();
        for (;;) {
          while (!best_literal_info_vec.empty()) {
            best_literal_to_add.reset(best_literal_info_vec.back());
            best_literal_info_vec.pop_back();
            if (!LiteralSelector::NeedRegrow(*building_state_->building_clause,
                                             *best_literal_to_add)) {
              QLOG << "The literal " << best_literal_to_add->literal.ToString()
                   << " references the last random literal in the building clause "
                   << building_state_->building_clause->ToString();

              break;
            }
            best_literal_to_add.reset();
          }

          if (best_literal_to_add == nullptr) {
            local_selector->GetNextBestLiterals(&best_literal_info_vec);
            if (best_literal_info_vec.empty()) {
              std::shared_ptr<QuickFoilState> previous_state = building_state_->previous_state;
              DCHECK(previous_state != nullptr);

              previous_state->black_random_literals.emplace(
                  building_state_->building_clause->CreateUnboundLastLiteral());

              // Note that literal_search_stats should be set to the stats for the current, not
              // the previous clause.
              literal_search_stats = building_state_->literal_search_stats;
              building_state_ = previous_state;
              local_selector = building_state_->literal_selector.get();
              QLOG << "Drop the last added literal, and regrow the previous clause "
                   << building_state_->building_clause->ToString();
              DCHECK(local_selector != nullptr);
              local_selector->GetNextBestLiterals(&best_literal_info_vec);
            }
          } else {
            break;
          }
        }
      }

      if (best_literal_to_add == nullptr) {
        LOG(ERROR) << "Cannot expand the current building clause: " << building_state_->building_clause->ToString();
        break;
      }

      for (const EvaluatedLiteralInfo* literal_info : best_literal_info_vec) {
        if (literal_info->num_covered_positive >
            FLAGS_minimum_coverage_for_tied_literal * local_num_uncovered_positives) {
          tied_literal_infos_.emplace_back(new TiedLiteralInfo(literal_info,
                                                               building_state_,
                                                               literal_search_stats));
        } else {
          delete literal_info;
        }
      }
      best_literal_info_vec.clear();

      if (AddBestCandidateLiteral(false,
                                  is_random_literal,
                                  best_literal_to_add.release(),
                                  literal_search_stats,
                                  &selector)) {
        break;
      }
    }

    ++current_outer_iterations_;
    if (!ContinueRuleSearch()) {
      break;
    }

    building_state_.reset();
    while (!tied_literal_infos_.empty()) {
      std::unique_ptr<TiedLiteralInfo> tied_literal_info(tied_literal_infos_.back().release());
      tied_literal_infos_.pop_back();
      building_state_ = tied_literal_info->building_state;
      std::unique_ptr<const EvaluatedLiteralInfo>* literal_info = &tied_literal_info->literal_info;

      QLOG << "Look at the tied literal " << (*literal_info)->literal.ToString()
           << " for clause " << building_state_->building_clause->ToString();

      // TODO(qzeng): Consider regrowing for tied literals.
      if (LiteralSelector::NeedRegrow(*building_state_->building_clause,
                                      **literal_info)) {
        QLOG << "Do not consider the tied literal " << (*literal_info)->literal.ToString()
             << " because regrowing is needed";
        building_state_.reset();
        continue;
      }

      if (!AddBestCandidateLiteral(true,
                                   false,
                                   literal_info->release(),
                                   tied_literal_info->literal_search_stats,
                                   nullptr)) {
        break;
      }
    }

    if (building_state_ == nullptr) {
      CreateMostGeneralBuildingClause();
    }
  }
}

// Return true if the building clause is finished.
bool QuickFoil::AddBestCandidateLiteral(const bool is_tied_literal,
                                        const bool is_random_literal,
                                        const EvaluatedLiteralInfo* best_literal_info_in,
                                        const std::shared_ptr<LiteralSearchStats>& literal_search_stats,
                                        std::unique_ptr<LiteralSelector>* selector) {
  DCHECK(best_literal_info_in != nullptr);
  QLOG << "Add literal " << best_literal_info_in->literal.ToString()
       << " (is_random=" << (is_random_literal ? "true" : "false")
       << ", num_covered_positive=" << best_literal_info_in->num_covered_positive << ", "
       << "num_covered_negative=" << best_literal_info_in->num_covered_negative << ", "
       << "num_binding_positive=" << best_literal_info_in->num_binding_positive << ", "
       << "num_binding_negative=" << best_literal_info_in->num_binding_negative << ", "
       << "precision=" << best_literal_info_in->GetPrecision() << ", "
       << "score=" << best_literal_info_in->score
       << ") to clause " << building_state_->building_clause->ToString();

  std::unique_ptr<const EvaluatedLiteralInfo> best_literal_info(best_literal_info_in);
  if (!is_random_literal && ShouldConsiderAsLastLiteral(*best_literal_info)) {
    if (AddBuildingClauseWithNewLiteral(best_literal_info.get())) {
      building_state_.reset();
      return true;
    } else if (is_tied_literal || building_state_->is_extended_from_tied_literal ||
               building_state_->building_clause->num_body_literals() >= FLAGS_maximum_clause_length) {
      QLOG << "Ignore the current building clause " << building_state_->building_clause->ToString()
           << " with the new literal " << best_literal_info->literal.ToString();
      building_state_.reset();
      return true;
    }
  }

  const FoilClauseConstSharedPtr new_building_clause =
      AddLiteralToBuildingClause(best_literal_info.get(), is_random_literal);

  if (is_random_literal) {
    DCHECK(selector != nullptr);
    building_state_->literal_selector.reset(selector->release());
    building_state_ = std::make_shared<QuickFoilState>((is_tied_literal || building_state_->is_extended_from_tied_literal),
                                                       new_building_clause,
                                                       building_state_,
                                                       literal_search_stats,
                                                       building_state_->black_random_literals,
                                                       building_state_->uncovered_positive_data);
  } else {
    building_state_ = std::make_shared<QuickFoilState>((is_tied_literal || building_state_->is_extended_from_tied_literal),
                                                       new_building_clause,
                                                       literal_search_stats,
                                                       building_state_->black_random_literals,
                                                       building_state_->uncovered_positive_data);
  }

  if (building_state_->building_clause->num_body_literals() == 1) {
    literal_serarch_stats_for_first_iteration_ = building_state_->literal_search_stats;
  }
  return false;
}

template <bool consider_random_literal>
void QuickFoil::EvaluateAllCandidateLiterals(
    const Vector<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>>& predicate_literal_info_groups,
    LiteralSelector* literal_selector,
    std::unordered_set<const FoilLiteral*>* pruned_literals_by_covered_results) {
  DCHECK_EQ(static_cast<int>(predicate_literal_info_groups.size()),
            building_state_->building_clause->num_variables());

  CandidateLiteralEvaluator evaluator(building_state_->building_clause);
  for (size_t i = 0; i < predicate_literal_info_groups.size(); ++i) {
    if (!predicate_literal_info_groups[i].empty()) {
      Vector<CandidateLiteralInfo*> candidate_literal_results;
      ElementDeleter<CandidateLiteralInfo> candidate_literal_results_deleter(&candidate_literal_results);
      evaluator.Evaluate(i, predicate_literal_info_groups[i], &candidate_literal_results);
      for (const CandidateLiteralInfo* literal_info : candidate_literal_results) {
        literal_selector->Insert<consider_random_literal>(*literal_info);
        if (literal_info->num_covered_positive == 0) {
          pruned_literals_by_covered_results->insert(literal_info->literal);
        }
      }
    }
  }
}

// Takes ownership of <literal_info_in>.
FoilClauseConstSharedPtr QuickFoil::AddLiteralToBuildingClause(const EvaluatedLiteralInfo* literal_info,
                                                               bool is_random) {
  const FoilClauseConstSharedPtr& building_clause = building_state_->building_clause;
  const FoilLiteral& literal = literal_info->literal;

  Vector<ConstBufferPtr> new_binding_table;
  CreateLabelAwareBindingTables(building_state_->building_clause,
                                literal,
                                literal_info->num_binding_positive,
                                literal_info->num_binding_negative,
                                &new_binding_table);

  const FoilClauseConstSharedPtr new_building_clause =
      building_clause->CopyWithAdditionalUnBoundBodyLiteral(
          literal,
          is_random,
          literal_info->num_binding_positive,
          literal_info->num_binding_negative,
          std::move(new_binding_table));

  QLOG << "New binding clause " << new_building_clause->ToString()
       << " (num_positive=" << new_building_clause->GetNumPositiveBindings() << ", "
       << "num_negative=" << new_building_clause->GetNumNegativeBindings() << ","
       << "num_random_literals=" << new_building_clause->GetNumRandomLiterals() << ")";

  return new_building_clause;
}

void QuickFoil::ComputeCoverageOnUncoveredData(const FoilLiteral& literal,
                                               std::unique_ptr<FoilHashTable>* positive_hash_table_for_coverage,
                                               size_type* num_positives_covered,
                                               size_type* num_negatives_covered,
                                               Vector<SemiJoinChunk*>* positive_coverage_results) {
  *num_positives_covered = 0;
  *num_negatives_covered = 0;

  std::unique_ptr<TableView> positive_table;
  std::unique_ptr<TableView> negative_table;

  if (building_state_->building_clause->IsBindingDataConseuctive()) {
    positive_table.reset(new TableView(
        building_state_->building_clause->CreatePositiveBlocks()));
    negative_table.reset(new TableView(
        building_state_->building_clause->CreateNegativeBlocks()));
  } else {
    positive_table.reset(new TableView(
        building_state_->building_clause->positive_blocks()));
    negative_table.reset(new TableView(
        building_state_->building_clause->negative_blocks()));
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
  for (int i = 0; i < target_predicate_->num_arguments(); ++i) {
    project_column_ids.emplace_back(i);
    coverage_join_keys.emplace_back(i);
  }

  const TableView& background_table = literal.predicate()->fact_table();
  std::unique_ptr<FoilHashTable> background_hash_table;
  {
    std::unique_ptr<FoilHashTable> positive_hash_table_for_bindings;
    std::unique_ptr<SemiJoin> binding_semijoin(
        SelectAndCreateSemiJoin(*positive_table,
                                background_table,
                                &positive_hash_table_for_bindings,
                                &background_hash_table,
                                clause_join_keys,
                                background_join_keys,
                                project_column_ids));
    positive_hash_table_for_coverage->reset(
        BuildHashTableAfterSemiJoin(positive_table->num_tuples(),
                                    target_predicate_->num_arguments(),
                                    binding_semijoin.release()));

    std::unique_ptr<SemiJoin> coverage_semijoin(
        CreateSemiJoin(true,
                       *building_state_->uncovered_positive_data,
                       *positive_table,
                       **positive_hash_table_for_coverage,
                       coverage_join_keys,
                       coverage_join_keys,
                       project_column_ids));
    std::unique_ptr<SemiJoinChunk> coverage_result(coverage_semijoin->Next());
    while (coverage_result != nullptr) {
      *num_positives_covered += coverage_result->num_ones;
      positive_coverage_results->emplace_back(coverage_result.release());
      coverage_result.reset(coverage_semijoin->Next());
    }
  }

  {
    std::unique_ptr<FoilHashTable> negative_hash_table_for_bindings;
    std::unique_ptr<SemiJoin> binding_semijoin(
        SelectAndCreateSemiJoin(*negative_table,
                                background_table,
                                &negative_hash_table_for_bindings,
                                &background_hash_table,
                                clause_join_keys,
                                background_join_keys,
                                project_column_ids));
    std::unique_ptr<FoilHashTable> negative_hash_table_for_coverage(
        BuildHashTableAfterSemiJoin(negative_table->num_tuples(),
                                    target_predicate_->num_arguments(),
                                    binding_semijoin.release()));

    std::unique_ptr<SemiJoin> coverage_semijoin(
        CreateSemiJoin(true,
                       *original_negative_data_,
                       *negative_table,
                       *negative_hash_table_for_coverage,
                       coverage_join_keys,
                       coverage_join_keys,
                       project_column_ids));
    std::unique_ptr<SemiJoinChunk> coverage_result(coverage_semijoin->Next());
    while (coverage_result != nullptr) {
      *num_negatives_covered += coverage_result->num_ones;
      coverage_result.reset(coverage_semijoin->Next());
    }
  }
}

bool QuickFoil::AddBuildingClauseWithNewLiteral(const EvaluatedLiteralInfo* literal_info) {

  const FoilLiteral& literal = literal_info->literal;

  DVLOG(3) << "Calculate the true precision for literal " << literal.ToString();

  Vector<SemiJoinChunk*> semi_join_positive_result;
  ElementDeleter<SemiJoinChunk> semi_join_positive_result_deleter(&semi_join_positive_result);

  size_type num_covered_local_positive;
  size_type num_covered_local_negative;
  std::unique_ptr<FoilHashTable> positive_hash_table_for_coverage;
  ComputeCoverageOnUncoveredData(literal,
                                 &positive_hash_table_for_coverage,
                                 &num_covered_local_positive,
                                 &num_covered_local_negative,
                                 &semi_join_positive_result);

  const double local_precision =
      static_cast<double>(num_covered_local_positive) / (num_covered_local_positive + num_covered_local_negative);
  const double local_recall =
      static_cast<double>(num_covered_local_positive) / static_cast<double>(building_state_->uncovered_positive_data->num_tuples());
  const double local_f_score =
      2 * local_precision*local_recall / static_cast<double>(local_precision + local_recall);

  QLOG << "Literal " << literal.ToString()
       << ": local_covered_positive=" << num_covered_local_positive << ", "
       << " local_covered_negative=" << num_covered_local_negative << ", "
       << " local_precision=" << local_precision << ", "
       << " local_recall=" << local_recall << ", "
       << " local_f-score=" << local_f_score;
  if (local_precision >= FLAGS_minimum_true_precision ||
      local_f_score >= FLAGS_minimum_f_score) {
    QLOG << "Add literal " << literal.ToString() << " and finish the current clause "
         << building_state_->building_clause->ToString() << ", because "
         << local_precision << "  " << local_f_score;

    Vector<BufferPtr> output_buffers;
    const size_type uncovered_num_tuples =
        building_state_->uncovered_positive_data->num_tuples() - num_covered_local_positive;
    const int num_target_arguments = target_predicate_->num_arguments();
    for (int i = 0; i < num_target_arguments; ++i) {
      output_buffers.emplace_back(
          std::make_shared<Buffer>(sizeof(cpp_type) * uncovered_num_tuples,
                                   uncovered_num_tuples));
    }

    if (building_state_->uncovered_positive_data != global_uncovered_positive_data_) {
      semi_join_positive_result_deleter.~ElementDeleter();

      std::unique_ptr<TableView> positive_table;

      if (building_state_->building_clause->IsBindingDataConseuctive()) {
        positive_table.reset(new TableView(
            building_state_->building_clause->CreatePositiveBlocks()));
      } else {
        positive_table.reset(new TableView(
            building_state_->building_clause->positive_blocks()));
      }

      Vector<AttributeReference> coverage_join_keys;
      Vector<int> project_column_ids;
      for (int i = 0; i < num_target_arguments; ++i) {
        coverage_join_keys.emplace_back(i);
        project_column_ids.emplace_back(i);
      }

      std::unique_ptr<SemiJoin> semi_join(
          CreateSemiJoin(true,
                         *global_uncovered_positive_data_,
                         *positive_table,
                         *positive_hash_table_for_coverage,
                         coverage_join_keys,
                         coverage_join_keys,
                         project_column_ids));

      size_type num_output_tuples = 0;
      std::unique_ptr<SemiJoinChunk> result(semi_join->Next());
      while (result != nullptr) {
        result->semi_bitvector.flip();
        result->num_ones = result->semi_bitvector.size() - result->num_ones;
        if (result->num_ones > 0) {
          for (int i = 0; i < num_target_arguments; ++i) {
            coverage_join_keys[i].EvaluateWithFilter(result->output_columns,
                                                     result->semi_bitvector,
                                                     result->num_ones,
                                                     num_output_tuples,
                                                     output_buffers[i].get());
          }
          num_output_tuples += result->num_ones;
        }
        result.reset(semi_join->Next());
      }

      DCHECK_GE(uncovered_num_tuples, num_output_tuples);
      if (uncovered_num_tuples != num_output_tuples) {
        for (int i = 0; i < num_target_arguments; ++i) {
          output_buffers[i]->Realloc(num_output_tuples * sizeof(cpp_type), num_output_tuples);
        }
      }
    } else {
      positive_hash_table_for_coverage.reset();

      Vector<AttributeReference> project_expressions;
      for (int i = 0; i < num_target_arguments; ++i) {
        project_expressions.emplace_back(i);
      }

      size_type num_output_tuples = 0;
      for (SemiJoinChunk* semi_join_result : semi_join_positive_result) {
        semi_join_result->semi_bitvector.flip();
        semi_join_result->num_ones =
            semi_join_result->semi_bitvector.size() - semi_join_result->num_ones;
        if (semi_join_result->num_ones > 0) {
          for (int i = 0; i < num_target_arguments; ++i) {
            project_expressions[i].EvaluateWithFilter(semi_join_result->output_columns,
                                                      semi_join_result->semi_bitvector,
                                                      semi_join_result->num_ones,
                                                      num_output_tuples,
                                                      output_buffers[i].get());
          }
          num_output_tuples += semi_join_result->num_ones;
        }
      }
      DCHECK_EQ(uncovered_num_tuples, num_output_tuples);
    }

    Vector<ConstBufferPtr> output_const_buffers;
    for (const BufferPtr& output_buffer : output_buffers) {
      output_const_buffers.emplace_back(
          std::make_shared<const ConstBuffer>(output_buffer));
    }

    global_uncovered_positive_data_.reset(new TableView(std::move(output_const_buffers)));

    std::unique_ptr<FoilClause> new_clause(building_state_->building_clause->CopyWithoutData());
    // The literal may actually be random, but we do not care.
    new_clause->AddUnBoundBodyLiteral(literal, false);
    learnt_clauses_.emplace_back(new_clause.release());
    QLOG << "New rule: " << learnt_clauses_.back()->ToString()
         << "(#Uncovered positive=" << global_uncovered_positive_data_->num_tuples() << ")";

    return true;
  }

  return false;
}

} /* namespace quickfoil */
