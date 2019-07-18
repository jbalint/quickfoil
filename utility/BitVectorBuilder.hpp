/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_BIT_VECTOR_BUILDER_HPP_
#define QUICKFOIL_UTILITY_BIT_VECTOR_BUILDER_HPP_

#include "utility/BitVector.hpp"
#include "utility/Macros.hpp"

#include "glog/logging.h"

namespace quickfoil {

class BitVectorBuilder {
 public:
  typedef BitVector::buffer_type buffer_type;
  typedef BitVector::block_type block_type;
  typedef BitVector::block_width_type block_width_type;
  static constexpr const block_width_type bits_per_block = BitVector::bits_per_block;

  BitVectorBuilder(BitVector* bit_vec)
      : bit_vector_(&bit_vec->m_bits),
        bits_in_last_block_(bit_vec->count_extra_bits()),
        num_blocks_((bits_in_last_block_ == 0? bit_vec->m_bits.size() : bit_vec->m_bits.size()-1)) {
    DCHECK(CheckLastBlock());
  }

  ~BitVectorBuilder() {
    DCHECK(CheckLastBlock());
  }

  const BitVector::size_type num_blocks() const {
    return num_blocks_;
  }

  const block_width_type bits_in_last_block() const {
    return bits_in_last_block_;
  }

  BitVector::buffer_type* bit_vector() {
    return bit_vector_;
  }

  bool CheckLastBlock() {
    if (bits_in_last_block_ > 0) {
      block_type const mask = (~static_cast<block_type>(0) << bits_in_last_block_);
      if ((bit_vector_->back() & mask) != 0) {
        return false;
      }
    }
    return true;
  }

 private:
  BitVector::buffer_type* bit_vector_;
  const block_width_type bits_in_last_block_;
  const BitVector::size_type num_blocks_;

  DISALLOW_COPY_AND_ASSIGN(BitVectorBuilder);
};

}  // namespace machine

#endif /* QUICKFOIL_UTILITY_BIT_VECTOR_BUILDER_HPP_ */
