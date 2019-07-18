/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/PartitionAssigner.hpp"

#include "gflags/gflags.h"

namespace quickfoil{

DEFINE_int32(partition_chunck_size,
             32768,
             "The number of tuples of a chunk in the PartitionAssigner.");

}  // namespace quickfoil

