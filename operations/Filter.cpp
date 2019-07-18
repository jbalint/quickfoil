/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/Filter.hpp"

#include <memory>

#include "learner/QuickFoilTimer.hpp"
#include "expressions/ComparisonPredicate.hpp"
#include "operations/HashJoin.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

FilterChunk* Filter::Next() {
  std::unique_ptr<HashJoinChunk> hash_join_chunk(hash_join_->Next());
  if (hash_join_chunk == nullptr) {
    return nullptr;
  }

  START_TIMER(QuickFoilTimer::kFilter);
  Vector<FoilFilterPredicate>* predicate_group =
      &predicate_groups_[hash_join_chunk->table_id][hash_join_chunk->join_group_id];
  bit_vectors_.resize(predicate_group->size());
  for (std::size_t i = 0; i < predicate_group->size(); ++i) {
    (*predicate_group)[i].EvaluateForJoin(hash_join_chunk->probe_columns,
                                          hash_join_chunk->build_columns,
                                          hash_join_chunk->probe_tids,
                                          hash_join_chunk->build_tids,
                                          &bit_vectors_[i]);
  }
  STOP_TIMER(QuickFoilTimer::kFilter);

  return new FilterChunk(hash_join_chunk.release(),
                         bit_vectors_);
}

}  // namespace quickfoil
