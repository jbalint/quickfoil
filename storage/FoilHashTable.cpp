/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "storage/FoilHashTable.hpp"

#include <cstdlib>
#include <memory>

#include "memory/Buffer.hpp"

#include "glog/logging.h"

namespace quickfoil {

#ifndef NEXT_POW_2
/**
 *  Taken from http://graphics.stanford.edu/~seander/bithacks.html.
 *  Note that V cannot be 0.
 */
#define NEXT_POW_2(V)                           \
    do {                                        \
        V--;                                    \
        V |= V >> 1;                            \
        V |= V >> 2;                            \
        V |= V >> 4;                            \
        V |= V >> 8;                            \
        V |= V >> 16;                           \
        V++;                                    \
    } while(false)
#endif

FoilHashTable::FoilHashTable(int size, uint32_t radix_bits) {
  DCHECK_GT(size, 0);

  int num_buckets = size;
  NEXT_POW_2(num_buckets);
  mask_ = (num_buckets - 1) << (radix_bits);

  const std::size_t next_buffer_size = sizeof(int) * size;
  const std::size_t buckets_buffer_size = sizeof(int) * num_buckets;

  buckets_buffer_.reset(new Buffer(calloc(num_buckets, sizeof(int)),
                                   buckets_buffer_size,
                                   num_buckets));

  next_buffer_.reset(new Buffer(next_buffer_size, next_buffer_size));
}

}  // namespace quickfoil
