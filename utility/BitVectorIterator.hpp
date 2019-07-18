/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_BIT_VECTOR_ITERATOR_HPP_
#define QUICKFOIL_UTILITY_BIT_VECTOR_ITERATOR_HPP_

#include <boost/pending/lowest_bit.hpp>

#include "utility/BitVector.hpp"
#include "utility/Macros.hpp"

namespace quickfoil {

class BitVectorIterator {
 public:
  typedef BitVector::size_type size_type;
  typedef BitVector::buffer_type buffer_type;
  typedef BitVector::block_type block_type;
  typedef BitVector::block_width_type block_width_type;

  BitVectorIterator(const BitVector& bit_vector)
      : bit_vector_(bit_vector.m_bits) {
    const BitVector::size_type first_pos = bit_vector.find_first();
    block_id_ = BitVector::block_index(first_pos);
    bit_id_ = BitVector::bit_index(first_pos);
    block_prefix_bit_ = block_id_ * BitVector::bits_per_block;
  }

  BitVector::size_type GetFirst() const {
    return block_prefix_bit_ + bit_id_;
  }

  BitVector::size_type FindNext() {
    if (bit_id_ < 63) {
      ++bit_id_;
      const block_type fore = bit_vector_[block_id_] >> bit_id_;
      if (fore != 0) {
        bit_id_ += static_cast<size_type>(::boost::lowest_bit(fore));
      } else {
        FindNewBlock();
      }
    } else {
      FindNewBlock();
    }
    return block_prefix_bit_ + bit_id_;
  }

 private:
  inline void FindNewBlock() {
    ++block_id_;
    while (bit_vector_[block_id_] == 0) {
      ++block_id_;
    }
    block_prefix_bit_ = block_id_ * BitVector::bits_per_block;
    bit_id_ = static_cast<size_type>(::boost::lowest_bit(bit_vector_[block_id_]));
  }

  const buffer_type& __restrict__ bit_vector_;
  size_type block_id_;
  block_width_type bit_id_;
  BitVector::size_type block_prefix_bit_;

  DISALLOW_COPY_AND_ASSIGN(BitVectorIterator);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_BIT_VECTOR_ITERATOR_HPP_ */
