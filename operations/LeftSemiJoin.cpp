/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "operations/LeftSemiJoin.hpp"

#include "gflags/gflags.h"

namespace quickfoil {

DEFINE_int32(semijoin_chunck_size,
             32768,
             "The number of tuples of a chunk in the LeftSemiJoin.");

}  // namespace quickfoil

