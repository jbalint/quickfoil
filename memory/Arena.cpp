/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "memory/Arena.hpp"

#include <algorithm>

namespace quickfoil {

Arena::Arena(size_t inital_buffer_size) {
  current_block_ = new ArenaBlock(inital_buffer_size);
  blocks_.emplace_back(current_block_);
}

void* Arena::Allocate(size_t size) {
  void* destination = current_block_->Allocate(size);
  if (destination != nullptr) {
    return destination;
  }

  size_t new_block_size = current_block_->size() * 2;
  if (new_block_size > MAX_ARENA_BUFFER_INCREMENT_SIZE) {
    new_block_size = MAX_ARENA_BUFFER_INCREMENT_SIZE;
  }
  current_block_ = new ArenaBlock(std::max(size, new_block_size));
  blocks_.emplace_back(current_block_);
  return current_block_->Allocate(size);
}

} /* namespace quickfoil */
