/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "learner/CandidateLiteralEvaluator.hpp"

#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "learner/CandidateLiteralInfo.hpp"
#include "learner/PredicateEvaluationPlan.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "operations/BuildHashTable.hpp"
#include "operations/CountAggregator.hpp"
#include "operations/Filter.hpp"
#include "operations/HashJoin.hpp"
#include "operations/PartitionAssigner.hpp"
#include "operations/RadixPartition.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilPredicate.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {
namespace {

  struct PredicateInfo {
    PredicateInfo(int left_predicate_id_in,
                  int right_predicate_id_in)
        : literal(nullptr),
          left_predicate_id(left_predicate_id_in),
          right_predicate_id(right_predicate_id_in),
          reference_count(0) {}

    PredicateInfo(const PredicateTreeNodePtr& plan_node_in)
        : literal(nullptr),
          plan_node(plan_node_in),
          left_predicate_id(-1),
          right_predicate_id(-1),
          reference_count(0) {}

    std::unordered_set<int> predicate_atoms;
    std::unordered_set<int> literal_ids;
    // The IDs of predicate tree nodes that have no common predicate with this one.
    std::unordered_set<int> mergeable_nodes;
    CandidateLiteralInfo* literal;
    PredicateTreeNodePtr plan_node;

    int left_predicate_id;
    int right_predicate_id;
    int reference_count;
  };

  struct LiteralInfo {
    std::unordered_set<int> predicate_atoms;
  };

  inline bool HasNoCommonPredicatesImpl(const std::unordered_set<int>& smaller_set,
                                        const std::unordered_set<int>& larger_set) {
    for (const int element : smaller_set) {
      if (larger_set.find(element) != larger_set.end()) {
        return false;
      }
    }
    return true;
  }

  bool HasNoCommonPredicates(const std::unordered_set<int>& left,
                             const std::unordered_set<int>& right) {
    if (left.size() > right.size()) {
      return HasNoCommonPredicatesImpl(right, left);
    }
    return HasNoCommonPredicatesImpl(left, right);
  }

  void SetIntersectionImpl(const std::unordered_set<int>& small,
                           const std::unordered_set<int>& large,
                           std::unordered_set<int>* result) {
    for (const int element : small) {
      if (large.find(element) != large.end()) {
        result->emplace(element);
      }
    }
  }

  void SetIntersection(const std::unordered_set<int>& left,
                       const std::unordered_set<int>& right,
                       std::unordered_set<int>* result) {
    if (left.size() > right.size()) {
      SetIntersectionImpl(right, left, result);
      return;
    }
    SetIntersectionImpl(left, right, result);
  }

}  // namespace

void CandidateLiteralEvaluator::GeneratePredicateEvaluationPlan(
    const Vector<CandidateLiteralInfo*>& literals,
    Vector<FoilFilterPredicate>* predicates,
    PredicateEvaluationPlan* literal_evaluation_tree) {
  DCHECK(!literals.empty());
  const int join_key = literals[0]->literal->join_key();

  Vector<PredicateInfo> predicate_tree_nodes;
  Vector<LiteralInfo> literal_info_vec(literals.size());
  std::map<std::pair<int, int>, int> attr_pair_id_map;
  std::unordered_set<int> remaining_literal_ids;
  int literal_id = 0;
  for (CandidateLiteralInfo* literal : literals) {
    int vid = 0;
    for (const FoilVariable& variable : literal->literal->variables()) {
      if (variable.IsBound() && vid != join_key) {
        std::pair<int, int> predicate_attr_pair(
            std::move(std::make_pair(vid, variable.variable_id())));
        std::map<std::pair<int, int>, int>::iterator it =
            attr_pair_id_map.find(predicate_attr_pair);
        if (it == attr_pair_id_map.end()) {
          literal_evaluation_tree->tree_nodes.emplace_back(
              new PredicateTreeNode());
          predicate_tree_nodes.emplace_back(literal_evaluation_tree->tree_nodes.back());
          predicate_tree_nodes.back().predicate_atoms.emplace(attr_pair_id_map.size());
          predicates->emplace_back(new AttributeReference(predicate_attr_pair.first),
                                   new AttributeReference(predicate_attr_pair.second));
          it = attr_pair_id_map.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(std::move(predicate_attr_pair)),
                                        std::forward_as_tuple(attr_pair_id_map.size())).first;
        }

        predicate_tree_nodes[it->second].literal_ids.emplace(literal_id);
        literal_info_vec[literal_id].predicate_atoms.emplace(it->second);
      }
      ++vid;
    }
    if (literal_info_vec[literal_id].predicate_atoms.empty()) {
      literal_evaluation_tree->literal = literal;
    } else if (literal_info_vec[literal_id].predicate_atoms.size() == 1u) {
      const int predicate_id = *literal_info_vec[literal_id].predicate_atoms.begin();
      literal_evaluation_tree->tree_nodes[predicate_id]->literal = literal;
      predicate_tree_nodes[predicate_id].literal_ids.erase(literal_id);
    } else {
      remaining_literal_ids.emplace(literal_id);
    }
    ++literal_id;
  }

  const int num_predicate_atoms = literal_evaluation_tree->tree_nodes.size();
  literal_evaluation_tree->num_atom_tree_nodes = num_predicate_atoms;

  for (int node_id = 0; node_id < static_cast<int>(predicate_tree_nodes.size()); ++node_id) {
    for (int i = 0; i < static_cast<int>(predicate_tree_nodes.size()); ++i) {
      if (i != node_id) {
        predicate_tree_nodes[node_id].mergeable_nodes.emplace(i);
      }
    }
  }

  while (!remaining_literal_ids.empty()) {
    int max_intersection_size = 0;
    int best_first_node_id = -1;
    int best_second_node_id = -1;
    for (int first_node_id = 0;
         first_node_id < static_cast<int>(predicate_tree_nodes.size()) - 1;
         ++first_node_id) {
      const std::unordered_set<int>& literal_set =
          predicate_tree_nodes[first_node_id].literal_ids;
      for (int second_node_id : predicate_tree_nodes[first_node_id].mergeable_nodes) {
        int intersection_size = 0;
        for (int literal_id : predicate_tree_nodes[second_node_id].literal_ids) {
          if (literal_set.find(literal_id) != literal_set.end()) {
            ++intersection_size;
          }
        }
        if (intersection_size > max_intersection_size) {
          best_first_node_id = first_node_id;
          best_second_node_id = second_node_id;
          max_intersection_size = intersection_size;
        }
      }
    }

    DCHECK_GT(max_intersection_size, 0);
    PredicateInfo* left_predicate_info =
        &predicate_tree_nodes[best_first_node_id];
    PredicateInfo* right_predicate_info =
        &predicate_tree_nodes[best_second_node_id];
    ++left_predicate_info->reference_count;
    ++right_predicate_info->reference_count;

    const int new_node_id = predicate_tree_nodes.size();
    predicate_tree_nodes.emplace_back(best_first_node_id, best_second_node_id);

    PredicateInfo* predicate_info = &predicate_tree_nodes.back();
    predicate_info->predicate_atoms = left_predicate_info->predicate_atoms;
    predicate_info->predicate_atoms.insert(right_predicate_info->predicate_atoms.begin(),
                                           right_predicate_info->predicate_atoms.end());
    SetIntersection(left_predicate_info->literal_ids,
                    right_predicate_info->literal_ids,
                    &predicate_info->literal_ids);

    for (std::unordered_set<int>::iterator check_literal_id_it = predicate_info->literal_ids.begin();
        check_literal_id_it != predicate_info->literal_ids.end();) {
      DCHECK(!(predicate_info->literal != nullptr &&
          literal_info_vec[*check_literal_id_it].predicate_atoms == predicate_info->predicate_atoms));
      if (predicate_info->literal == nullptr &&
          literal_info_vec[*check_literal_id_it].predicate_atoms == predicate_info->predicate_atoms) {
        predicate_info->literal = literals[*check_literal_id_it];
        remaining_literal_ids.erase(*check_literal_id_it);
        for (int i = 0; i < new_node_id; ++i) {
          predicate_tree_nodes[i].literal_ids.erase(*check_literal_id_it);
        }
        check_literal_id_it = predicate_info->literal_ids.erase(check_literal_id_it);
      } else {
        ++check_literal_id_it;
      }
    }

    for (int i = 0; i < new_node_id; ++i) {
      if (i != best_first_node_id &&
          i != best_second_node_id &&
          HasNoCommonPredicates(predicate_info->predicate_atoms,
                                predicate_tree_nodes[i].predicate_atoms)) {
        predicate_tree_nodes[i].mergeable_nodes.emplace(new_node_id);
      }
    }
  }

  for (int node_id = static_cast<int>(predicate_tree_nodes.size()) - 1;
       node_id >= num_predicate_atoms;
       --node_id) {
    if (predicate_tree_nodes[node_id].literal == nullptr &&
        predicate_tree_nodes[node_id].reference_count == 0) {
      --predicate_tree_nodes[predicate_tree_nodes[node_id].left_predicate_id].reference_count;
      --predicate_tree_nodes[predicate_tree_nodes[node_id].right_predicate_id].reference_count;
    }
  }

  for (Vector<PredicateInfo>::iterator it = predicate_tree_nodes.begin() + num_predicate_atoms;
       it < predicate_tree_nodes.end();
       ++it) {
    if (it->literal != nullptr ||
        it->reference_count != 0) {
      literal_evaluation_tree->tree_nodes.emplace_back(
          std::make_shared<ConjunctivePredicateTreeNode>(
              predicate_tree_nodes[it->left_predicate_id].plan_node,
              predicate_tree_nodes[it->right_predicate_id].plan_node));
      literal_evaluation_tree->tree_nodes.back()->literal = it->literal;
      it->plan_node = literal_evaluation_tree->tree_nodes.back();
    }
  }

  DVLOG(2) << OutputPredicateEvaluationPlan(literals,
                                            *predicates,
                                            *literal_evaluation_tree);
}

void CandidateLiteralEvaluator::Evaluate(
    int clause_join_key_id,
    const std::unordered_map<const FoilPredicate*,
                             Vector<const FoilLiteral*>>& literal_groups,
    Vector<CandidateLiteralInfo*>* results) {

  Vector<const TableView*> background_tables;
  Vector<Vector<Vector<FoilFilterPredicate>>> predicate_groups(literal_groups.size());
  Vector<Vector<PredicateEvaluationPlan>> predicate_plan_groups(literal_groups.size());
  Vector<Vector<int>> literal_join_keys(literal_groups.size());

  Vector<Vector<Vector<FoilFilterPredicate>>>::iterator predicate_groups_it =
      predicate_groups.begin();
  Vector<Vector<PredicateEvaluationPlan>>::iterator predicate_plan_groups_it =
      predicate_plan_groups.begin();
  Vector<Vector<int>>::iterator literal_join_keys_it = literal_join_keys.begin();

  for (auto& literal_group : literal_groups) {
    background_tables.emplace_back(&literal_group.first->fact_table());

    std::unordered_map<int, Vector<CandidateLiteralInfo*>> join_key_to_literals;
    for (const FoilLiteral* literal : literal_group.second) {
      results->emplace_back(new CandidateLiteralInfo(literal));
      join_key_to_literals[literal->join_key()].emplace_back(
          results->back());
    }

    for (auto& join_key_and_literals : join_key_to_literals) {
      const FoilLiteral* candidate_literal =
          join_key_and_literals.second[0]->literal;
      const TableView& fact_table =
          candidate_literal->predicate()->fact_table();
      if (fact_table.partitions_at(candidate_literal->join_key()).empty()) {
        RadixPartition(candidate_literal->join_key(),
                       const_cast<TableView*>(&fact_table));
      }

      literal_join_keys_it->emplace_back(candidate_literal->join_key());
      predicate_groups_it->emplace_back();
      predicate_plan_groups_it->emplace_back();
      START_TIMER(QuickFoilTimer::kGeneratePlans);
      GeneratePredicateEvaluationPlan(join_key_and_literals.second,
                                      &predicate_groups_it->back(),
                                      &predicate_plan_groups_it->back());
      STOP_TIMER(QuickFoilTimer::kGeneratePlans);
    }

    ++predicate_groups_it;
    ++predicate_plan_groups_it;
    ++literal_join_keys_it;
  }

  if (building_clause_->IsBindingDataConseuctive()) {
    TableView binding_table(building_clause_->integral_blocks());
    START_TIMER(QuickFoilTimer::kPartitionAndBuildBindings);
    RadixPartition(clause_join_key_id,
                   &binding_table);
    BuildHashTableOnPartitions(clause_join_key_id,
                               &binding_table);
    STOP_TIMER(QuickFoilTimer::kPartitionAndBuildBindings);

    std::unique_ptr<PartitionAssigner> assigner(
        new PartitionAssigner(std::move(background_tables),
                              std::move(literal_join_keys)));
    std::unique_ptr<HashJoin> hash_join(
        new HashJoin(binding_table,
                     clause_join_key_id,
                     assigner.release()));
    std::unique_ptr<Filter> filter(
        new Filter(std::move(predicate_groups),
                   hash_join.release()));
    std::unique_ptr<CountAggregator> aggregator(
        new CountAggregator(filter.release(),
                            std::move(predicate_plan_groups)));

    START_TIMER(QuickFoilTimer::kEvaluateLiterals);
    aggregator->Execute(building_clause_->GetNumPositiveBindings());
    STOP_TIMER(QuickFoilTimer::kEvaluateLiterals);
    return;
  }

  {
    TableView positive_table(std::move(building_clause_->positive_blocks()));
    START_TIMER(QuickFoilTimer::kPartitionAndBuildBindings);
    RadixPartition(clause_join_key_id,
                   &positive_table);
    BuildHashTableOnPartitions(clause_join_key_id,
                               &positive_table);
    STOP_TIMER(QuickFoilTimer::kPartitionAndBuildBindings);

    std::unique_ptr<PartitionAssigner> assigner(
        new PartitionAssigner(background_tables,
                              literal_join_keys));
    std::unique_ptr<HashJoin> hash_join(
        new HashJoin(positive_table,
                     clause_join_key_id,
                     assigner.release()));
    std::unique_ptr<Filter> filter(
        new Filter(predicate_groups,
                   hash_join.release()));

    Vector<Vector<PredicateEvaluationPlan>> predicate_plan_groups_clone;;
    for (const Vector<PredicateEvaluationPlan>& predicate_plan_group : predicate_plan_groups) {
      predicate_plan_groups_clone.emplace_back();
      for (const PredicateEvaluationPlan& predicate_plan : predicate_plan_group) {
        std::unique_ptr<PredicateEvaluationPlan> clone(predicate_plan.Clone());
        predicate_plan_groups_clone.back().emplace_back(std::move(*clone));
      }
    }

    std::unique_ptr<CountAggregator> aggregator(
        new CountAggregator(filter.release(),
                            std::move(predicate_plan_groups_clone)));

    START_TIMER(QuickFoilTimer::kEvaluateLiterals);
    aggregator->ExecuteOnPositives();
    STOP_TIMER(QuickFoilTimer::kEvaluateLiterals);
  }

  TableView negative_table(std::move(building_clause_->negative_blocks()));
  RadixPartition(clause_join_key_id,
                 &negative_table);
  BuildHashTableOnPartitions(clause_join_key_id,
                             &negative_table);

  std::unique_ptr<PartitionAssigner> assigner(
      new PartitionAssigner(std::move(background_tables),
                            std::move(literal_join_keys)));
  std::unique_ptr<HashJoin> hash_join(
      new HashJoin(negative_table,
                   clause_join_key_id,
                   assigner.release()));
  std::unique_ptr<Filter> filter(
      new Filter(std::move(predicate_groups),
                 hash_join.release()));
  std::unique_ptr<CountAggregator> aggregator(
      new CountAggregator(filter.release(),
                          std::move(predicate_plan_groups)));

  START_TIMER(QuickFoilTimer::kEvaluateLiterals);
  aggregator->ExecuteOnNegatives();
  STOP_TIMER(QuickFoilTimer::kEvaluateLiterals);
}

std::string CandidateLiteralEvaluator::OutputPredicateEvaluationPlan(
    const Vector<CandidateLiteralInfo*>& literals,
    const Vector<FoilFilterPredicate>& predicates,
    const PredicateEvaluationPlan& literal_evaluation_plan) {
  std::ostringstream out;
  out << "Candidate literals: \n";
  for (const CandidateLiteralInfo* literal : literals) {
    out << "\t" << literal->literal->ToString() << "\n";
  }

  out << "Predicates: \n";
  int predicate_id = 0;
  for (const FoilFilterPredicate& predicate : predicates) {
    out << "\tPredicate " << predicate_id <<": "
        << predicate.build_attribute().column_id()
        << " " << predicate.build_attribute().column_id() << "\n";
  }

  out << "Predicate evaluation plan: \n";
  if (literal_evaluation_plan.literal != nullptr) {
    out << "\tRoot: " <<  literal_evaluation_plan.literal->literal->ToString();
  }

  std::unordered_map<const PredicateTreeNode*, int> node_id_map;
  for (int i = 0; i < literal_evaluation_plan.num_atom_tree_nodes; ++i) {
    out << "\tPredicate " << i << ": ";
    if (literal_evaluation_plan.tree_nodes[i]->literal == nullptr) {
      out << "null\n";
    } else {
      out << literal_evaluation_plan.tree_nodes[i]->literal->literal->ToString() << "\n";
    }
    node_id_map.emplace(literal_evaluation_plan.tree_nodes[i].get(),
                        i);
  }
  for (int i = literal_evaluation_plan.num_atom_tree_nodes;
       i < static_cast<int>(literal_evaluation_plan.tree_nodes.size());
       ++i) {
    const ConjunctivePredicateTreeNode* tree_node =
        static_cast<const ConjunctivePredicateTreeNode*>(literal_evaluation_plan.tree_nodes[i].get());
    out << "\tConjunction " << i <<" ("
        << node_id_map[tree_node->left_node.get()] << ", "
        << node_id_map[tree_node->right_node.get()] << "): ";
    if (tree_node->literal == nullptr) {
      out << "null";
    } else {
      out << tree_node->literal->literal->ToString();
    }
    out << "\n";
    node_id_map.emplace(tree_node, i);
  }

  return out.str();
}

}  // namespace quickfoil
