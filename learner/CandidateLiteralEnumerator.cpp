/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "learner/CandidateLiteralEnumerator.hpp"

#include <unordered_map>

#include "learner/CandidateLiteralInfo.hpp"
#include "learner/LiteralSearchStats.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilPredicate.hpp"
#include "schema/FoilVariable.hpp"
#include "utility/StringUtil.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

void CandidateLiteralEnumerator::EnumerateForMostGeneralClause(
    const FoilClause& building_clause,
    const LiteralSearchStats& last_run_stats,
    std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
    std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals) {
  if (last_run_stats.generated_candidate_literals == nullptr) {
    EnumerateForMostGeneralClauseFromScratch(building_clause,
                                             entire_generated_literals,
                                             pruned_generated_literals);
  } else {
    for (const auto& predicate_literal_group_pair : *last_run_stats.generated_candidate_literals) {
      Vector<FoilLiteral>* generated_literals_for_predicate = &(*entire_generated_literals)[predicate_literal_group_pair.first];
      for (const FoilLiteral& generated_literal : predicate_literal_group_pair.second) {
        if (last_run_stats.pruned_literals_by_covered_results->find(&generated_literal) ==
            last_run_stats.pruned_literals_by_covered_results->end()) {
          generated_literals_for_predicate->emplace_back(generated_literal);
        }
      }
      Vector<const FoilLiteral*>* pruned_literals_for_predicate = &(*pruned_generated_literals)[predicate_literal_group_pair.first];
      for (const FoilLiteral& literal : *generated_literals_for_predicate) {
        pruned_literals_for_predicate->emplace_back(&literal);
      }
    }
  }
}

void CandidateLiteralEnumerator::EnumerateForMostGeneralClauseFromScratch(
    const FoilClause& building_clause,
    std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
    std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals) {
  DCHECK(building_clause.body_literals().empty());

  std::unordered_map<int, Vector<FoilVariable>> variable_type_to_variable_map;
  for (const FoilVariable& variable : building_clause.variables()) {
    variable_type_to_variable_map[variable.variable_type_id()].emplace_back(variable);
  }

  for (const FoilPredicate* background_predicate : background_predicates_) {
    DCHECK_LT(background_predicate->id(), static_cast<int>(background_predicates_.size()));
    Vector<FoilLiteral> literals_with_new_vars_only;

    Vector<Vector<FoilVariable>> variables_per_argument;
    GenerateVariableVectorForPredicate<false>(
        variable_type_to_variable_map,
        background_predicate,
        FoilVariableSet(),
        &variables_per_argument);

    literals_with_new_vars_only.emplace_back(background_predicate);
    // candidate_literals_for_predicate cannot have any literals identical to existing ones.
    GenerateCandidateLiterals(0, variables_per_argument, &literals_with_new_vars_only);
    DCHECK(literals_with_new_vars_only.back().AreAllVariablesUnBound());
    literals_with_new_vars_only.pop_back();  // Remove the literal with all variables unbound.

    if (!literals_with_new_vars_only.empty()) {
      const Vector<FoilLiteral>* immutable_generated_literals_for_predicate = nullptr;
      if (background_predicate != building_clause.head_literal().predicate()) {
        auto insert_result = entire_generated_literals->emplace(std::piecewise_construct,
                                                                std::forward_as_tuple(background_predicate),
                                                                std::forward_as_tuple(std::move(literals_with_new_vars_only)));
        immutable_generated_literals_for_predicate = &insert_result.first->second;
      } else {
        Vector<FoilLiteral>* generated_literals_for_predicate = &(*entire_generated_literals)[background_predicate];
        immutable_generated_literals_for_predicate = generated_literals_for_predicate;
        for (const FoilLiteral& literal : literals_with_new_vars_only) {
          if (!literal.Equals(building_clause.head_literal())) {
            generated_literals_for_predicate->emplace_back(literal);
          }
        }
      }

      Vector<const FoilLiteral*>* pruned_literals_for_predicate = &(*pruned_generated_literals)[background_predicate];
      for (const FoilLiteral& literal : *immutable_generated_literals_for_predicate) {
        pruned_literals_for_predicate->emplace_back(&literal);
      }
    }
  }
}

template <bool has_key>
void CandidateLiteralEnumerator::GenerateAndPruneCandidateLiteralForPredicate(
    const FoilClause& building_clause,
    const LiteralSearchStats& last_run_stats,
    const FoilPredicate* background_predicate,
    const std::unordered_map<int, Vector<FoilVariable>>& variable_type_to_variable_map,
    const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map,
    std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
    std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals) {
  DCHECK_LT(background_predicate->id(), static_cast<int>(background_predicates_.size()));
  DCHECK(!has_key || background_predicate->key() >= 0);

  const Vector<const FoilLiteral*>* body_literals_with_predicate = nullptr;
  const std::unordered_map<int, Vector<const FoilLiteral*>>::const_iterator predicate_to_body_literals_map_it =
      predicate_to_body_literals_map.find(background_predicate->id());
  if (predicate_to_body_literals_map_it != predicate_to_body_literals_map.end()) {
    body_literals_with_predicate = &predicate_to_body_literals_map_it->second;
  }

  FoilVariableSet key_variables;
  if (has_key) {
    if (body_literals_with_predicate != nullptr) {
      for (const FoilLiteral* body_literal : *body_literals_with_predicate) {
        key_variables.emplace(body_literal->variable_at(background_predicate->key()));
      }
    }
  }

  Vector<FoilLiteral> literals_with_new_vars_only;
  Vector<Vector<FoilVariable>> variables_per_argument;

  if (has_key && !key_variables.empty()) {
    GenerateVariableVectorForPredicate<true>(
        variable_type_to_variable_map,
        background_predicate,
        key_variables,
        &variables_per_argument);
  } else {
    GenerateVariableVectorForPredicate<false>(
        variable_type_to_variable_map,
        background_predicate,
        key_variables,
        &variables_per_argument);
  }

  literals_with_new_vars_only.emplace_back(background_predicate);
  // candidate_literals_for_predicate cannot have any literals identical to existing ones.
  GenerateCandidateLiterals(0, variables_per_argument, &literals_with_new_vars_only);
  DCHECK(literals_with_new_vars_only.back().AreAllVariablesUnBound());
  literals_with_new_vars_only.pop_back();  // Remove the literal with all variables unbound.

  auto emplace_result = entire_generated_literals->emplace(std::piecewise_construct,
                                                          std::forward_as_tuple(background_predicate),
                                                          std::forward_as_tuple(std::move(literals_with_new_vars_only)));
  Vector<FoilLiteral>* entire_candidate_literals_for_predicate =
      &emplace_result.first->second;

  DCHECK(last_run_stats.generated_candidate_literals != nullptr);
  DCHECK(last_run_stats.pruned_literals_by_covered_results != nullptr);

  const std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>::const_iterator found_it =
      last_run_stats.generated_candidate_literals->find(background_predicate);
  if (found_it != last_run_stats.generated_candidate_literals->end()) {
    for (const FoilLiteral& old_literal : found_it->second) {
      if (last_run_stats.pruned_literals_by_covered_results->find(&old_literal) ==
          last_run_stats.pruned_literals_by_covered_results->end()) {
        Vector<FoilLiteral> new_extended_candidate_literals;
        new_extended_candidate_literals.emplace_back(background_predicate);
        GenerateCandidateLiteralsFromAnother(old_literal,
                                             0,
                                             variables_per_argument,
                                             &new_extended_candidate_literals);
        new_extended_candidate_literals.pop_back();

        entire_candidate_literals_for_predicate->insert(
            entire_candidate_literals_for_predicate->end(),
            new_extended_candidate_literals.begin(),
            new_extended_candidate_literals.end());
        if (!building_clause.body_literals().back().Equals(old_literal)) {
          entire_candidate_literals_for_predicate->emplace_back(old_literal);
        }
      }
    }
  }

  Vector<const FoilLiteral*>* pruned_cadidate_literals_for_predicate = &(*pruned_generated_literals)[background_predicate];
  if (body_literals_with_predicate == nullptr) {
    for (const FoilLiteral& literal : *entire_candidate_literals_for_predicate) {
      pruned_cadidate_literals_for_predicate->emplace_back(&literal);
    }
  } else {
    for (const FoilLiteral& candidate_literal : *entire_candidate_literals_for_predicate) {
      if (!CheckReplaceableDuplicate(building_clause, candidate_literal, predicate_to_body_literals_map)) {
        pruned_cadidate_literals_for_predicate->emplace_back(&candidate_literal);
      }
    }
  }
}

// TODO(qzeng): Incorporate dynamic pruning.
void CandidateLiteralEnumerator::EnumerateForNonMostGeneralClause(
    const FoilClause& building_clause,
    const LiteralSearchStats& last_run_stats,
    std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>* entire_generated_literals,
    std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>* pruned_generated_literals) {
  DCHECK(!building_clause.body_literals().empty());
  canonical_databases_.reserve(background_predicates_.size());

  int num_variables;
  if (last_run_stats.generated_candidate_literals == nullptr) {
    num_variables = 0;
  } else {
    num_variables = last_run_stats.generated_candidate_literals->size();
  }

  std::unordered_map<int, Vector<FoilVariable>> variable_type_to_variable_map;

  const FoilLiteral& new_literal = building_clause.body_literals().back();
  for (const FoilVariable& variable : new_literal.variables()) {
    if (variable.variable_id() >= building_clause.num_variables_without_last_body_literal()) {
      ++num_variables;
      variable_type_to_variable_map[variable.variable_type_id()].emplace_back(variable);
    }
  }

  std::unordered_map<int, Vector<const FoilLiteral*>> predicate_to_body_literals_map;
  for (const FoilLiteral& body_literal : building_clause.body_literals()) {
    predicate_to_body_literals_map[body_literal.predicate()->id()].push_back(&body_literal);
  }

  for (const FoilPredicate* background_predicate : background_predicates_) {
    if (background_predicate->key() >= 0) {
      GenerateAndPruneCandidateLiteralForPredicate<true>(
          building_clause,
          last_run_stats,
          background_predicate,
          variable_type_to_variable_map,
          predicate_to_body_literals_map,
          entire_generated_literals,
          pruned_generated_literals);
    } else {
      GenerateAndPruneCandidateLiteralForPredicate<false>(
          building_clause,
          last_run_stats,
          background_predicate,
          variable_type_to_variable_map,
          predicate_to_body_literals_map,
          entire_generated_literals,
          pruned_generated_literals);
    }
  }

  canonical_databases_.clear();
}

// FIXME(qzeng): Unbound variables are currently all considered as distinct.
template <bool has_key>
void CandidateLiteralEnumerator::GenerateVariableVectorForPredicate(
    const std::unordered_map<int, Vector<FoilVariable>>& variable_type_to_variable_map,
    const FoilPredicate* predicate,
    const FoilVariableSet& key_variables,
    Vector<Vector<FoilVariable>>* variables_per_argument) const {
  variables_per_argument->reserve(predicate->num_arguments());
  DCHECK(!has_key || !key_variables.empty());

  for (int i = 0; i < predicate->num_arguments(); ++i) {
    const std::unordered_map<int, Vector<FoilVariable>>::const_iterator
    candidate_variables_it =
        variable_type_to_variable_map.find(predicate->argument_type_at(i));
    if (candidate_variables_it != variable_type_to_variable_map.end()) {
      if (has_key) {
        variables_per_argument->emplace_back();
        Vector<FoilVariable>& pruned_candidate_variables = variables_per_argument->back();
        for (const FoilVariable& variable : candidate_variables_it->second) {
          if (key_variables.find(variable) == key_variables.end()) {
            pruned_candidate_variables.emplace_back(variable);
          }
        }
      } else {
        variables_per_argument->emplace_back(candidate_variables_it->second);
      }
    } else {
      variables_per_argument->emplace_back();
    }
    (*variables_per_argument)[i].emplace_back(predicate->argument_type_at(i));
  }
}

void CandidateLiteralEnumerator::GenerateCandidateLiteralsFromAnother(
    const FoilLiteral& base_literal,
    int num_arguments_filled,
    const Vector<Vector<FoilVariable>>& variables_per_argument,
    Vector<FoilLiteral>* candidate_literals) const {
  DCHECK_LT(num_arguments_filled, static_cast<int>(variables_per_argument.size()));

  if (base_literal.variable_at(num_arguments_filled).IsBound()) {
    for (FoilLiteral& candidate_literal : *candidate_literals) {
      candidate_literal.AddVariable(base_literal.variable_at(num_arguments_filled));
    }
  } else {
    const Vector<FoilVariable>& candidate_variables =
        variables_per_argument[num_arguments_filled];
    DCHECK(!candidate_variables.empty());

    Vector<FoilLiteral> new_candidate_literals;
    for (FoilLiteral& literal : *candidate_literals) {
      for (size_t i = 1; i < candidate_variables.size(); ++i) {
        new_candidate_literals.emplace_back(literal);
        new_candidate_literals.back().AddVariable(candidate_variables[i]);
      }
      literal.AddVariable(candidate_variables[0]);
    }

    candidate_literals->insert(candidate_literals->end(),
                               new_candidate_literals.begin(),
                               new_candidate_literals.end());

  }

  ++num_arguments_filled;

  if (num_arguments_filled  == static_cast<int>(variables_per_argument.size())) {
    return;
  }

  GenerateCandidateLiteralsFromAnother(base_literal,
                                       num_arguments_filled,
                                       variables_per_argument,
                                       candidate_literals);
}

// FIXME(qzeng): Unbound variables are currently all considered as distinct.
void CandidateLiteralEnumerator::GenerateCandidateLiterals(
    int num_arguments_filled,
    const Vector<Vector<FoilVariable>>& variables_per_argument,
    Vector<FoilLiteral>* candidate_literals) const {
  DCHECK_LT(num_arguments_filled, static_cast<int>(variables_per_argument.size()));
  const Vector<FoilVariable>& candidate_variables =
      variables_per_argument[num_arguments_filled];
  DCHECK(!candidate_variables.empty());

  Vector<FoilLiteral> new_candidate_literals;
  for (FoilLiteral& literal : *candidate_literals) {
    for (size_t i = 1; i < candidate_variables.size(); ++i) {
      new_candidate_literals.emplace_back(literal);
      new_candidate_literals.back().AddVariable(candidate_variables[i]);
    }
    literal.AddVariable(candidate_variables[0]);
  }

  candidate_literals->insert(candidate_literals->end(),
                             new_candidate_literals.begin(),
                             new_candidate_literals.end());
  ++num_arguments_filled;

  if (num_arguments_filled  == static_cast<int>(variables_per_argument.size())) {
    return;
  }

  GenerateCandidateLiterals(num_arguments_filled, variables_per_argument, candidate_literals);
}

bool CandidateLiteralEnumerator::CheckReplaceableDuplicate(
    const FoilClause& clause,
    const FoilLiteral& literal,
    const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map) {
  const auto literal_vec_it = predicate_to_body_literals_map.find(literal.predicate()->id());
  DCHECK(literal_vec_it != predicate_to_body_literals_map.end());

  if (!literal_vec_it->second.empty()) {
    std::unordered_map<int, Vector<Vector<int>>>::const_iterator canonical_db_it =
        canonical_databases_.find(literal.predicate()->id());
    if (canonical_db_it == canonical_databases_.end()) {
#ifdef ENUMERATOR_VERBOSE
      DVLOG(4) << "Create a canonical database for predicate " << literal.predicate()->name();
#endif
      canonical_db_it = CreateCanonicalDatabase(clause,
                                                *literal.predicate(),
                                                predicate_to_body_literals_map);
    }
#ifdef ENUMERATOR_VERBOSE
    DVLOG(4) << "\nClause: " << clause.ToString() << "\n"
             << "Predicate excluded: "<< literal.predicate()->name() << "\n"
             << "Canonical db without the predicate: "
             << VectorOfVectorToString(canonical_db_it->second);
#endif

    Vector<Vector<int>> canonical_predicate_relation_without_new_literal;
    for (const FoilLiteral* existing_literal : literal_vec_it->second) {
      AddRowToCanonicalRelationForExistingLiteral(
          *existing_literal, &canonical_predicate_relation_without_new_literal);
    }
#ifdef ENUMERATOR_VERBOSE
    DVLOG(4) << "Canonical relation without new literal: "
             << VectorOfVectorToString(canonical_predicate_relation_without_new_literal);
#endif
    for (size_t literal_index = 0;
         literal_index < literal_vec_it->second.size();
         ++literal_index) {
      Vector<Vector<int>> canonical_joined_relation =
          canonical_db_it->second;
      for (size_t i = 0; i < literal_vec_it->second.size(); ++i) {
        if (i != literal_index) {
          NestedLoopsJoin<false, false>(*literal_vec_it->second[i],
                                        canonical_predicate_relation_without_new_literal,
                                        &canonical_joined_relation);
        }
      }
      if (NestedLoopsJoin<true, true>(literal,
                                      canonical_predicate_relation_without_new_literal,
                                      &canonical_joined_relation)) {
        Vector<Vector<int>> canonical_predicate_relation_with_new_literal(
            canonical_predicate_relation_without_new_literal.begin(),
            canonical_predicate_relation_without_new_literal.begin() + literal_index);
        canonical_predicate_relation_with_new_literal.insert(
            canonical_predicate_relation_with_new_literal.end(),
            canonical_predicate_relation_without_new_literal.begin() + literal_index + 1,
            canonical_predicate_relation_without_new_literal.end());

        AddRowToCanonicalRelationForNewLiteral(literal,
                                               clause.num_variables(),
                                               &canonical_predicate_relation_with_new_literal);
#ifdef ENUMERATOR_VERBOSE
        DVLOG(4) << "Canonical relation with new literal and without existing literal "
                 << literal_vec_it->second[literal_index]->ToString() << ": "
                 << VectorOfVectorToString(canonical_predicate_relation_with_new_literal);
#endif

        canonical_joined_relation = canonical_db_it->second;
        for (size_t i = 0; i < literal_vec_it->second.size() - 1; ++i) {
          NestedLoopsJoin<false, false>(*literal_vec_it->second[i],
                                        canonical_predicate_relation_with_new_literal,
                                        &canonical_joined_relation);
        }
        if (NestedLoopsJoin<false, true>(*literal_vec_it->second.back(),
                                         canonical_predicate_relation_with_new_literal,
                                          &canonical_joined_relation)) {
#ifdef ENUMERATOR_VERBOSE
          DVLOG(3) << "Literal " << literal.ToString() << " is a replaceable duplicate of "
                   << literal_vec_it->second[literal_index]->ToString() << " in the clause "
                   << clause.ToString();
#endif
          return true;
        }
      }
    }
  }

  // TODO(qzeng): Use conflict relationships between background predicates to remove literals.
  return false;
}

std::unordered_map<int, Vector<Vector<int>>>::const_iterator
CandidateLiteralEnumerator::CreateCanonicalDatabase(
    const FoilClause& clause,
    const FoilPredicate& predicate,
    const std::unordered_map<int, Vector<const FoilLiteral*>>& predicate_to_body_literals_map) {
  Vector<Vector<int>> canonical_joined_relation;
  const int num_variables = clause.num_variables();
  bool has_joined_head_literal = false;

  for (const auto map_pair : predicate_to_body_literals_map) {
    if (map_pair.first != predicate.id()) {
      Vector<Vector<int>> canonical_predicate_relation;
      for (const FoilLiteral* body_literal : map_pair.second) {
        AddRowToCanonicalRelationForExistingLiteral(
            *body_literal,
            &canonical_predicate_relation);
      }

      if (!has_joined_head_literal &&
          clause.head_literal().predicate()->id() == map_pair.first) {
        has_joined_head_literal = true;
        AddRowToCanonicalRelationForExistingLiteral(
            clause.head_literal(),
            &canonical_predicate_relation);
      }
      DCHECK(!canonical_predicate_relation.empty());
      if (canonical_joined_relation.empty()) {
        const FoilLiteral& first_literal = *map_pair.second[0];
        for (const Vector<int>& row : canonical_predicate_relation) {
          canonical_joined_relation.emplace_back(num_variables, -1);
          for (size_t i = 0; i < row.size(); ++i) {
            canonical_joined_relation.back()[first_literal.variable_at(i).variable_id()] = row[i];
          }
        }
        for (size_t i = 1; i < map_pair.second.size(); ++i) {
          NestedLoopsJoin<false, false>(
              *map_pair.second[i],
              canonical_predicate_relation,
              &canonical_joined_relation);
        }
      } else {
        for (const FoilLiteral* literal : map_pair.second) {
          NestedLoopsJoin<false, false>(
              *literal,
              canonical_predicate_relation,
              &canonical_joined_relation);
        }
      }
    }
  }

  if (!has_joined_head_literal) {
    if (canonical_joined_relation.empty()) {
      canonical_joined_relation.emplace_back(num_variables, -1);
      for (int i = 0; i < clause.head_literal().num_variables(); ++i) {
        const int vid = clause.head_literal().variable_at(i).variable_id();
        canonical_joined_relation.back()[vid] = vid;
      }
    } else {
      Vector<Vector<int>> canonical_predicate_relation;
      AddRowToCanonicalRelationForExistingLiteral(
          clause.head_literal(),
          &canonical_predicate_relation);
      NestedLoopsJoin<false, false>(
          clause.head_literal(),
          canonical_predicate_relation,
          &canonical_joined_relation);
    }
  }

  return canonical_databases_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(predicate.id()),
      std::forward_as_tuple(std::move(canonical_joined_relation))).first;
}

template <bool check_bound_variable, bool return_if_any>
bool CandidateLiteralEnumerator::NestedLoopsJoin(
    const FoilLiteral& literal,
    const Vector<Vector<int>>& canonical_predicate_rel,
    Vector<Vector<int>>* canonical_joined_rel) const {
  Vector<Vector<int>> new_canonical_joined_rel;
  for (const Vector<int>& predicate_row : canonical_predicate_rel) {
    for (const Vector<int>& joined_row : *canonical_joined_rel) {
      size_t i;
      for (i = 0; i < predicate_row.size(); ++i) {
        if (check_bound_variable) {
          if (!literal.variable_at(i).IsBound()) {
            continue;
          }
        }
        if (joined_row[literal.variable_at(i).variable_id()] != -1 &&
            joined_row[literal.variable_at(i).variable_id()] != predicate_row[i]) {
          break;
        }
      }
      if (i == predicate_row.size()) {
        if (return_if_any) {
          return true;
        }

        Vector<int> new_row = joined_row;
        for (i = 0; i < predicate_row.size(); ++i) {
          if (new_row[literal.variable_at(i).variable_id()] == -1) {
            new_row[literal.variable_at(i).variable_id()] = predicate_row[i];
          }
        }
        new_canonical_joined_rel.emplace_back(std::move(new_row));
      }
    }
  }
#ifdef ENUMERATOR_VERBOSE
  DVLOG(5) << "Literal: " << literal.ToString() << "\n"
           << "Joined rel: " << VectorOfVectorToString(*canonical_joined_rel) << "\n"
           << "Predicate rel: " << VectorOfVectorToString(canonical_predicate_rel) << "\n"
           << "Result: " << VectorOfVectorToString(new_canonical_joined_rel);
#endif
  *canonical_joined_rel = std::move(new_canonical_joined_rel);
  return !canonical_joined_rel->empty();
}

void CandidateLiteralEnumerator::AddRowToCanonicalRelationForExistingLiteral(
    const FoilLiteral& literal,
    Vector<Vector<int>>* canonical_rel) const {
  canonical_rel->emplace_back();
  Vector<int>& row = canonical_rel->back();
  for (const FoilVariable& variable : literal.variables()) {
      row.emplace_back(variable.variable_id());
  }
}

void CandidateLiteralEnumerator::AddRowToCanonicalRelationForNewLiteral(
    const FoilLiteral& literal,
    int new_variable_start_id,
    Vector<Vector<int>>* canonical_rel) const {
  canonical_rel->emplace_back();
  Vector<int>& row = canonical_rel->back();
  for (const FoilVariable& variable : literal.variables()) {
    if (variable.IsBound()) {
      row.emplace_back(variable.variable_id());
    } else {
      row.emplace_back(new_variable_start_id++);
    }
  }
}

} /* namespace quickfoil */
