/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
#include "memory/MemoryUsage.hpp"

#include "memory/MemoryConfig.hpp"

#include "gflags/gflags.h"

namespace quickfoil {
namespace {
constexpr const std::size_t default_memory_quota_value = static_cast<std::size_t>(TOTAL_PHYSICAL_MEMORY) * 1024 * 1024;
}

DEFINE_uint64(memory_quota,
              default_memory_quota_value,
              "The memory quota for buffer-managed data (includes memory for partitions or hash tables)");

}  // namespace quickfoil
#endif
