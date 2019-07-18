/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_STORAGE_FOIL_HASH_TABLE_HPP_
#define QUICKFOIL_STORAGE_FOIL_HASH_TABLE_HPP_

#include <cstdlib>
#include <memory>

#include "memory/Buffer.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"

namespace quickfoil {

// Chained hash table.
class FoilHashTable {
 public:
  FoilHashTable()
      : mask_(0) {
  }

  FoilHashTable(int size, uint32_t radix_bits);

  FoilHashTable(FoilHashTable&& other)
      : mask_(other.mask_),
        next_buffer_(std::move(other.next_buffer_)),
        buckets_buffer_(std::move(other.buckets_buffer_)) {}

  const int* next() const {
    return next_buffer_->as_type<int>();
  }

  const int* buckets() const {
    return buckets_buffer_->as_type<int>();
  }

  int* mutable_next() {
    return next_buffer_->mutable_as_type<int>();
  }

  int* mutable_buckets() {
    return buckets_buffer_->mutable_as_type<int>();
  }

  const uint32_t mask() const {
    return mask_;
  }

 private:
  uint32_t mask_ = 0;

  std::unique_ptr<Buffer> next_buffer_;
  std::unique_ptr<Buffer> buckets_buffer_;

  DISALLOW_COPY_AND_ASSIGN(FoilHashTable);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_STORAGE_FOIL_HASH_TABLE_HPP_ */
