/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_MEMORY_MEMORY_USAGE_HPP_
#define QUICKFOIL_MEMORY_MEMORY_USAGE_HPP_

#include <cstdlib>

#include "utility/Macros.hpp"

#include "gflags/gflags.h"

#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR

#define LOG_ALLOC(size) MemoryUsage::GetInstance()->Allocate(size)
#define LOG_DEALLOC(size) MemoryUsage::GetInstance()->Deallocate(size)

#else

#define LOG_ALLOC(size)
#define LOG_DEALLOC(size)

#endif

namespace quickfoil {

#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR

DECLARE_uint64(memory_quota);

class MemoryUsage {
 public:
  static MemoryUsage* GetInstance() {
    static MemoryUsage instance;
    return &instance;
  }

  inline void Allocate(const std::size_t size) {
    memory_usage_ += size;
  }

  inline void Deallocate(const std::size_t size) {
    memory_usage_ -= size;
  }

  inline std::size_t memory_usage() const {
    return memory_usage_;
  }

  inline double GetMemoryUsageInGB() const {
    return memory_usage_ / (1024.0 * 1024 * 1024);
  }

  inline bool NotExceedQuotaWithNewAllocation(const std::size_t size) {
    return memory_usage_ + size < FLAGS_memory_quota;
  }

 private:
  MemoryUsage() {}

  std::size_t memory_usage_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MemoryUsage);
};

#endif /* QUICKFOIL_ENABLE_MEMORY_MONITOR */

}  // namespace quickfoil

#endif /* QUICKFOIL_MEMORY_MEMORYUSAGE_HPP_ */
