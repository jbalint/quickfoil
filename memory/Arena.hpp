/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_MEMORY_ARENA_HPP_
#define QUICKFOIL_MEMORY_ARENA_HPP_

#include <memory>
#include <vector>

#include "memory/MemUtil.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "folly/Range.h"

namespace quickfoil {

class Arena {
 public:
  explicit Arena(size_t inital_buffer_size = INITIAL_ARENA_BUFFER_SIZE);

  const char* AddStringPiece(const folly::StringPiece str);

 private:
  class ArenaBlock;

  void* Allocate(size_t size);

  Vector<std::unique_ptr<ArenaBlock>> blocks_;
  ArenaBlock* current_block_ = nullptr;

  static constexpr size_t INITIAL_ARENA_BUFFER_SIZE = 1 * 1024;
  static constexpr size_t MAX_ARENA_BUFFER_INCREMENT_SIZE = 32 * 1024 * 1024;

  DISALLOW_COPY_AND_ASSIGN(Arena);
};

class Arena::ArenaBlock {
 public:
  ArenaBlock(size_t buffer_size)
      : buffer_(qf_malloc(buffer_size)),
        buffer_size_(buffer_size) {}

  ~ArenaBlock() {
    free(buffer_);
  }

  void* Allocate(size_t size) {
    if (offset_ + size < buffer_size_) {
      void* ret = static_cast<char*>(buffer_) + offset_;
      offset_ += size;
      return ret;
    }
    return nullptr;
  }

  size_t size() const {
    return buffer_size_;
  }

 private:
  void* buffer_;
  std::size_t buffer_size_;
  std::size_t offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ArenaBlock);
};

inline const char* Arena::AddStringPiece(const folly::StringPiece str) {
  const size_t str_size = str.size() + 1;
  void* destination = Allocate(str_size);
  ::memcpy(destination, str.data(), str_size);
  return static_cast<const char*>(destination);
}

} /* namespace quickfoil */

#endif /* QUICKFOIL_MEMORY_ARENA_HPP_ */
