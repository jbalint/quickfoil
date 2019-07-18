/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/CountAggregator.hpp"

#include <cstddef>
#include <memory>

#include "learner/CandidateLiteralInfo.hpp"
#include "learner/PredicateEvaluationPlan.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "operations/Filter.hpp"
#include "operations/HashJoin.hpp"
#include "utility/BitVector.hpp"
#include "utility/BitVectorBuilder.hpp"
#include "utility/BitVectorIterator.hpp"
#include "utility/ElementDeleter.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {
namespace {

template <bool positive, bool negative>
void ResetSemiVectors(std::size_t num_binding_tuples,
                      PredicateEvaluationPlan* plan) {
  if (plan->literal != nullptr) {
    if (positive) {
      plan->positive_semi_bitvector.resize(num_binding_tuples);
      plan->positive_semi_bitvector.reset();
    }
    if (negative) {
      plan->negative_semi_bitvector.resize(num_binding_tuples);
      plan->negative_semi_bitvector.reset();
    }
  }

  for (PredicateTreeNodePtr& tree_node : plan->tree_nodes) {
    if (tree_node->literal != nullptr) {
      if (positive) {
        tree_node->positive_semi_bitvector.resize(num_binding_tuples);
        tree_node->positive_semi_bitvector.reset();
      }
      if (negative) {
        tree_node->negative_semi_bitvector.resize(num_binding_tuples);
        tree_node->negative_semi_bitvector.reset();
      }
    }
  }
}

}  // namespace

void CountAggregator::LabelBitVector(const size_type num_positive,
                                     const Vector<size_type>& build_tids,
                                     BitVector* bit_vector) const {
  BitVectorBuilder result_builder(bit_vector);
  BitVectorBuilder::buffer_type::iterator raw_bit_vector_iterator =
      result_builder.bit_vector()->begin();
  Vector<size_type>::const_iterator tid_it = build_tids.begin();
  for (std::size_t block_id = 0; block_id < result_builder.num_blocks(); ++block_id) {
    for (unsigned bit = 0; bit < 64; ++bit) {
      *raw_bit_vector_iterator |=
          (static_cast<BitVectorBuilder::block_type>(*tid_it < num_positive) << bit);
      ++tid_it;
    }
    ++raw_bit_vector_iterator;
  }

  for (unsigned bit = 0; bit < result_builder.bits_in_last_block(); ++bit) {
    *raw_bit_vector_iterator |=
        (static_cast<BitVectorBuilder::block_type>(*tid_it < num_positive) << bit);
    ++tid_it;
  }
}

void CountAggregator::UpdateSemiBitVector(const Vector<size_type>& build_relative_tids,
                                          const BitVector& join_bitvector,
                                          const std::size_t num_ones,
                                          size_type* __restrict__ count,
                                          BitVector* __restrict__ semi_bitvector) const {
  if (num_ones > 0) {
    DCHECK_EQ(build_relative_tids.size(), join_bitvector.size());
    BitVectorIterator bv_it(join_bitvector);
    if (!semi_bitvector->test_set(build_relative_tids[bv_it.GetFirst()])) {
      ++*count;
    }
    for (std::size_t i = 1; i < num_ones; ++i) {
      if (!semi_bitvector->test_set(build_relative_tids[bv_it.FindNext()])) {
        ++*count;
      }
    }
  }
}

void CountAggregator::UpdateSemiBitVectorWithNoFilter(
    const Vector<size_type>& build_relative_tids,
    size_type* __restrict__ count,
    BitVector* __restrict__ semi_bitvector) const {
  for (size_type tid : build_relative_tids) {
    if (!semi_bitvector->test_set(tid)) {
      ++*count;
    }
  }
}

void CountAggregator::Execute(const size_type num_positive) {
  std::unique_ptr<FilterChunk> filter_chunk(filter_->Next());
  DCHECK(filter_chunk != nullptr);

  do {
    const HashJoinChunk* hash_join_chunk = filter_chunk->hash_join_chunk.get();

    START_TIMER(QuickFoilTimer::kCount);
    PredicateEvaluationPlan* evaluation_plan =
        &score_plans_[hash_join_chunk->table_id][hash_join_chunk->join_group_id];
    if (evaluation_plan->saved_partition_id != hash_join_chunk->partition_id) {
      ResetSemiVectors<true, true>(hash_join_chunk->binding_partition_size,
                                   evaluation_plan);
      evaluation_plan->saved_partition_id = hash_join_chunk->partition_id;
    }

    if (evaluation_plan->num_atom_tree_nodes == 0) {
      // Fast path.
      CandidateLiteralInfo* __restrict__ root_literal = evaluation_plan->literal;
      BitVector* __restrict__ positive_semi_bitvector =
          &evaluation_plan->positive_semi_bitvector;
      BitVector* __restrict__ negative_semi_bitvector =
          &evaluation_plan->negative_semi_bitvector;
      DCHECK(root_literal != nullptr);
      const std::size_t num_tuples = hash_join_chunk->build_tids.size();
      const Vector<size_type>& build_tids = hash_join_chunk->build_tids;
      const Vector<size_type>& build_relative_tids = hash_join_chunk->build_relative_tids;
      for (std::size_t i = 0; i < num_tuples; ++i) {
        if (build_tids[i] < num_positive) {
          ++root_literal->num_binding_positive;
          if (!positive_semi_bitvector->test_set(build_relative_tids[i])) {
            ++root_literal->num_covered_positive;
          }
        } else {
          ++root_literal->num_binding_negative;
          if (!negative_semi_bitvector->test_set(build_relative_tids[i])) {
            ++root_literal->num_covered_negative;
          }
        }
      }
    } else {
      BitVector unfiltered_positive_bit_vector(hash_join_chunk->build_tids.size());
      LabelBitVector(num_positive,
                     hash_join_chunk->build_tids,
                     &unfiltered_positive_bit_vector);
      BitVector unfiltered_negative_bit_vector = ~unfiltered_positive_bit_vector;

      if (evaluation_plan->literal != nullptr) {
        const size_type num_positives = unfiltered_positive_bit_vector.count();
        const size_type num_negative = hash_join_chunk->build_tids.size() - num_positives;

        evaluation_plan->literal->num_binding_positive += num_positives;
        evaluation_plan->literal->num_binding_negative += num_negative;
        UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                            unfiltered_positive_bit_vector,
                            num_positives,
                            &evaluation_plan->literal->num_covered_positive,
                            &evaluation_plan->positive_semi_bitvector);
        UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                            unfiltered_negative_bit_vector,
                            num_negative,
                            &evaluation_plan->literal->num_covered_negative,
                            &evaluation_plan->negative_semi_bitvector);
      }

      const Vector<BitVector>& bit_vectors = filter_chunk->bit_vectors;
      DCHECK_EQ(evaluation_plan->num_atom_tree_nodes, static_cast<int>(bit_vectors.size()));
      for (int i = 0; i < evaluation_plan->num_atom_tree_nodes; ++i) {
        const PredicateTreeNodePtr& node = evaluation_plan->tree_nodes[i];
        node->bit_vector = &bit_vectors[i];
        if (node->literal != nullptr) {
          const BitVector positive_bitvector =
              *node->bit_vector & unfiltered_positive_bit_vector;
          const BitVector negative_bitvector =
              *node->bit_vector & unfiltered_negative_bit_vector;

          const size_type num_positives = positive_bitvector.count();
          const size_type num_negative = negative_bitvector.count();

          node->literal->num_binding_positive += num_positives;
          node->literal->num_binding_negative += num_negative;
          UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                              positive_bitvector,
                              num_positives,
                              &node->literal->num_covered_positive,
                              &node->positive_semi_bitvector);
          UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                              negative_bitvector,
                              num_negative,
                              &node->literal->num_covered_negative,
                              &node->negative_semi_bitvector);
        }
      }

      Vector<BitVector*> tmp_bit_vectors;
      ElementDeleter<BitVector> tmp_bit_vectors_deliter(&tmp_bit_vectors);
      for (int i = evaluation_plan->num_atom_tree_nodes;
           i < static_cast<int>(evaluation_plan->tree_nodes.size());
           ++i) {
        ConjunctivePredicateTreeNode* node =
            static_cast<ConjunctivePredicateTreeNode*>(evaluation_plan->tree_nodes[i].get());
        tmp_bit_vectors.emplace_back(
            new BitVector(std::move(*node->left_node->bit_vector & *node->right_node->bit_vector)));
        node->bit_vector = tmp_bit_vectors.back();
        if (node->literal != nullptr) {
          const BitVector positive_bitvector =
              *node->bit_vector & unfiltered_positive_bit_vector;
          const BitVector negative_bitvector =
              *node->bit_vector & unfiltered_negative_bit_vector;

          const size_type num_positives = positive_bitvector.count();
          const size_type num_negative = negative_bitvector.count();

          node->literal->num_binding_positive += num_positives;
          node->literal->num_binding_negative += num_negative;
          UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                              positive_bitvector,
                              num_positives,
                              &node->literal->num_covered_positive,
                              &node->positive_semi_bitvector);
          UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                              negative_bitvector,
                              num_negative,
                              &node->literal->num_covered_negative,
                              &node->negative_semi_bitvector);
        }
      }
    }
    STOP_TIMER(QuickFoilTimer::kCount);

    filter_chunk.reset(filter_->Next());
  } while (filter_chunk != nullptr);
}


template <bool positive>
void CountAggregator::ExecuteOnOneLabel() {
  std::unique_ptr<FilterChunk> filter_chunk(filter_->Next());
  DCHECK(filter_chunk != nullptr);

  do {
    START_TIMER(QuickFoilTimer::kCount);

    const HashJoinChunk* hash_join_chunk = filter_chunk->hash_join_chunk.get();
    PredicateEvaluationPlan* evaluation_plan =
        &score_plans_[hash_join_chunk->table_id][hash_join_chunk->join_group_id];
    if (evaluation_plan->saved_partition_id != hash_join_chunk->partition_id) {
      ResetSemiVectors<positive, !positive>(hash_join_chunk->binding_partition_size,
                                            evaluation_plan);
      evaluation_plan->saved_partition_id = hash_join_chunk->partition_id;
    }

    if (evaluation_plan->num_atom_tree_nodes == 0) {
      // Fast path.
      CandidateLiteralInfo* __restrict__ root_literal = evaluation_plan->literal;
      DCHECK(root_literal != nullptr);
      if (positive) {
        BitVector* __restrict__ positive_semi_bitvector =
            &evaluation_plan->positive_semi_bitvector;
        root_literal->num_binding_positive += hash_join_chunk->build_tids.size();
        for (size_type relative_tid : hash_join_chunk->build_relative_tids) {
          if (!positive_semi_bitvector->test_set(relative_tid)) {
            ++root_literal->num_covered_positive;
          }
        }
      } else {
        BitVector* __restrict__ negative_semi_bitvector =
            &evaluation_plan->negative_semi_bitvector;
        root_literal->num_binding_negative += hash_join_chunk->build_tids.size();
        for (size_type relative_tid : hash_join_chunk->build_relative_tids) {
          if (!negative_semi_bitvector->test_set(relative_tid)) {
            ++root_literal->num_covered_negative;
          }
        }
      }
    } else {
      if (evaluation_plan->literal != nullptr) {
        if (positive) {
          evaluation_plan->literal->num_binding_positive += hash_join_chunk->build_tids.size();
          UpdateSemiBitVectorWithNoFilter(hash_join_chunk->build_relative_tids,
                                          &evaluation_plan->literal->num_covered_positive,
                                          &evaluation_plan->positive_semi_bitvector);
        } else {
          evaluation_plan->literal->num_binding_negative += hash_join_chunk->build_tids.size();
          UpdateSemiBitVectorWithNoFilter(hash_join_chunk->build_relative_tids,
                                          &evaluation_plan->literal->num_covered_negative,
                                          &evaluation_plan->negative_semi_bitvector);
        }
      }

      const Vector<BitVector>& bit_vectors = filter_chunk->bit_vectors;
      DCHECK_EQ(evaluation_plan->num_atom_tree_nodes, static_cast<int>(bit_vectors.size()));
      for (int i = 0; i < evaluation_plan->num_atom_tree_nodes; ++i) {
        const PredicateTreeNodePtr& node = evaluation_plan->tree_nodes[i];
        node->bit_vector = &bit_vectors[i];
        if (node->literal != nullptr) {
          if (positive) {
            const size_type num_positives = node->bit_vector->count();
            node->literal->num_binding_positive += num_positives;
            UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                                *node->bit_vector,
                                num_positives,
                                &node->literal->num_covered_positive,
                                &node->positive_semi_bitvector);
          } else {
            const size_type num_negative = node->bit_vector->count();
            node->literal->num_binding_negative += num_negative;
            UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                                *node->bit_vector,
                                num_negative,
                                &node->literal->num_covered_negative,
                                &node->negative_semi_bitvector);
          }
        }
      }

      Vector<BitVector*> tmp_bit_vectors;
      ElementDeleter<BitVector> tmp_bit_vectors_deliter(&tmp_bit_vectors);
      for (int i = evaluation_plan->num_atom_tree_nodes;
           i < static_cast<int>(evaluation_plan->tree_nodes.size());
           ++i) {
        ConjunctivePredicateTreeNode* node =
            static_cast<ConjunctivePredicateTreeNode*>(evaluation_plan->tree_nodes[i].get());
        tmp_bit_vectors.emplace_back(
            new BitVector(std::move(*node->left_node->bit_vector & *node->right_node->bit_vector)));
        node->bit_vector = tmp_bit_vectors.back();
        if (node->literal != nullptr) {
          if (positive) {
            const size_type num_positives = node->bit_vector->count();
            node->literal->num_binding_positive += num_positives;
            UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                                *node->bit_vector,
                                num_positives,
                                &node->literal->num_covered_positive,
                                &node->positive_semi_bitvector);
          } else {
            const size_type num_negative = node->bit_vector->count();
            node->literal->num_binding_negative += num_negative;
            UpdateSemiBitVector(hash_join_chunk->build_relative_tids,
                                *node->bit_vector,
                                num_negative,
                                &node->literal->num_covered_negative,
                                &node->negative_semi_bitvector);
          }
        }
      }
    }
    STOP_TIMER(QuickFoilTimer::kCount);
    filter_chunk.reset(filter_->Next());
  } while (filter_chunk != nullptr);
}

void CountAggregator::ExecuteOnPositives() {
  ExecuteOnOneLabel<true>();
}

void CountAggregator::ExecuteOnNegatives() {
  ExecuteOnOneLabel<false>();
}

}  // namespace quickfoil
