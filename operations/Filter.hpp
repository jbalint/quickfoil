/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_FILTER_HPP_
#define QUICKFOIL_OPERATIONS_FILTER_HPP_

#include <memory>

#include "expressions/ComparisonPredicate.hpp"
#include "operations/HashJoin.hpp"
#include "utility/BitVector.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

struct FilterChunk {
  FilterChunk(HashJoinChunk* hash_join_chunk_in,
              const Vector<BitVector>& bit_vectors_in)
      : hash_join_chunk(hash_join_chunk_in),
        bit_vectors(bit_vectors_in) {}

  std::unique_ptr<HashJoinChunk> hash_join_chunk;
  const Vector<BitVector>& bit_vectors;
};

class Filter {
 public:
  Filter(Vector<Vector<Vector<FoilFilterPredicate>>>&& predicate_groups,
         HashJoin* hash_join)
      : predicate_groups_(std::move(predicate_groups)),
        hash_join_(hash_join) {}

  Filter(const Vector<Vector<Vector<FoilFilterPredicate>>>& predicate_groups,
         HashJoin* hash_join)
      : predicate_groups_(predicate_groups),
        hash_join_(hash_join) {}

  FilterChunk* Next();

 private:
  Vector<Vector<Vector<FoilFilterPredicate>>> predicate_groups_;
  Vector<BitVector> bit_vectors_;
  std::unique_ptr<HashJoin> hash_join_;

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_FILTER_HPP_ */
