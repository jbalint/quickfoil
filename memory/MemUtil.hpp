/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_MEMORY_MEMUTIL_HPP_
#define QUICKFOIL_MEMORY_MEMUTIL_HPP_

#include <cstddef>
#include <cstdlib>
#include <immintrin.h>

#include "memory/MemoryUsage.hpp"
#include "memory/MemoryConfig.hpp"

namespace quickfoil {

inline void* qf_malloc(std::size_t size) {
  LOG_ALLOC(size);
  return malloc(size);
}

inline void* qf_calloc(std::size_t num, std::size_t size) {
  LOG_ALLOC(size);
  return calloc(num, size);
}

#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
inline void* qf_realloc(void* ptr, std::size_t old_size, std::size_t new_size) {
  LOG_DEALLOC(old_size);
  LOG_ALLOC(new_size);
  return realloc(ptr, new_size);
}
#else
inline void* qf_realloc(void* ptr, std::size_t new_size) {
  return realloc(ptr, new_size);
}
#endif

inline void* qf_aligned_alloc(std::size_t alignment, std::size_t size) {
  LOG_ALLOC(size);
#ifdef HAVE_C11_ALIGNED_ALLOC
  return aligned_alloc(alignment, size);
#else

#ifdef HAVE_POSIX_MEMALIGN
  void* destination = nullptr;
  posix_memalign(&destination, alignment, size);
  return destination;
#else
  return nullptr;
#endif

#endif
}

inline void* cacheline_aligned_alloc(std::size_t size) {
  return qf_aligned_alloc(CACHE_LINE_SIZE, size);
}

inline void cacheline_memcpy(void* __restrict__ dst, void* __restrict__ src) {
#ifdef __AVX__
  __m256i* d1 = (__m256i*)dst;
  __m256i s1 = *((__m256i*)src);
  __m256i* d2 = d1 + 1;
  __m256i s2 = *(((__m256i*)src) + 1);

  _mm256_stream_si256(d1, s1);
  _mm256_stream_si256(d2, s2);
#elif defined(__SSE2__)
  __m128i* d1= (__m128i*)dst;
  __m128i* d2 = d1 + 1;
  __m128i* d3 = d1 + 2;
  __m128i* d4 = d1 + 3;
  __m128i s1 = *(__m128i*)src;
  __m128i s2 = *((__m128i*)src + 1);
  __m128i s3 = *((__m128i*)src + 2);
  __m128i s4 = *((__m128i*)src + 3);

  _mm_stream_si128(d1, s1);
  _mm_stream_si128(d2, s2);
  _mm_stream_si128(d3, s3);
  _mm_stream_si128(d4, s4);
#else
  memcpy(dest, src, 64);
#endif
}

}

#endif /* QUICKFOIL_MEMORY_MEMUTIL_HPP_ */
