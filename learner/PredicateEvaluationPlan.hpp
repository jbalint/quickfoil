/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_PREDICATE_EVALUATION_PLAN_HPP_
#define QUICKFOIL_LEARNER_PREDICATE_EVALUATION_PLAN_HPP_

#include <memory>
#include <unordered_map>

#include "utility/BitVector.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class CandidateLiteralInfo;

struct PredicateTreeNode;
typedef std::shared_ptr<PredicateTreeNode> PredicateTreeNodePtr;

class ConjunctivePredicateTreeNode;
typedef std::shared_ptr<ConjunctivePredicateTreeNode> ConjunctivePredicateTreeNodePtr;

class AtomPredicateTreeNode;
typedef std::shared_ptr<AtomPredicateTreeNode> AtomPredicateTreeNodePtr;

struct PredicateTreeNode {
  PredicateTreeNode()
      : bit_vector(nullptr) {}

  PredicateTreeNode(CandidateLiteralInfo* literal_in)
      : bit_vector(nullptr),
        literal(literal_in) {}

  virtual ~PredicateTreeNode() {}

  virtual PredicateTreeNode* Clone(
      const std::unordered_map<const PredicateTreeNode*, PredicateTreeNodePtr>& substitution_map) const {
    return new PredicateTreeNode(literal);
  }

  const BitVector* bit_vector;
  CandidateLiteralInfo* literal = nullptr;
  BitVector positive_semi_bitvector;
  BitVector negative_semi_bitvector;
};

struct ConjunctivePredicateTreeNode : public PredicateTreeNode {
  ConjunctivePredicateTreeNode(const PredicateTreeNodePtr& left_node_in,
                               const PredicateTreeNodePtr& right_node_in)
      : left_node(left_node_in),
        right_node(right_node_in) {}

  ConjunctivePredicateTreeNode(CandidateLiteralInfo* literal,
                               const PredicateTreeNodePtr& left_node_in,
                               const PredicateTreeNodePtr& right_node_in)
      : PredicateTreeNode(literal),
        left_node(left_node_in),
        right_node(right_node_in) {}

  PredicateTreeNode* Clone(
        const std::unordered_map<const PredicateTreeNode*, PredicateTreeNodePtr>& substitution_map) const override {
    std::unordered_map<const PredicateTreeNode*, PredicateTreeNodePtr>::const_iterator left_it =
        substitution_map.find(left_node.get());
    std::unordered_map<const PredicateTreeNode*, PredicateTreeNodePtr>::const_iterator right_it =
        substitution_map.find(right_node.get());
    DCHECK(left_it != substitution_map.end());
    DCHECK(right_it != substitution_map.end());
    return new ConjunctivePredicateTreeNode(literal,
                                            left_it->second,
                                            right_it->second);
  }

  PredicateTreeNodePtr left_node;
  PredicateTreeNodePtr right_node;
};

struct PredicateEvaluationPlan {
  PredicateEvaluationPlan()
      : literal(nullptr) {}

  PredicateEvaluationPlan(PredicateEvaluationPlan&& other)
      : literal(other.literal),
        tree_nodes(std::move(other.tree_nodes)),
        num_atom_tree_nodes(other.num_atom_tree_nodes) {
  }

  PredicateEvaluationPlan* Clone() const {
    std::unique_ptr<PredicateEvaluationPlan> clone(new PredicateEvaluationPlan);
    clone->literal = literal;
    clone->num_atom_tree_nodes = num_atom_tree_nodes;

    std::unordered_map<const PredicateTreeNode*, PredicateTreeNodePtr> substitution_map;
    for (const PredicateTreeNodePtr& tree_node : tree_nodes) {
      clone->tree_nodes.emplace_back(tree_node->Clone(substitution_map));
      substitution_map.emplace(tree_node.get(), clone->tree_nodes.back());
    }
    return clone.release();
  }

  CandidateLiteralInfo* literal;
  BitVector positive_semi_bitvector;
  BitVector negative_semi_bitvector;
  int saved_partition_id = -1;

  Vector<PredicateTreeNodePtr> tree_nodes;
  int num_atom_tree_nodes = 0;
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_PREDICATE_EVALUATION_PLAN_HPP_ */
