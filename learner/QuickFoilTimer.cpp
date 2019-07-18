/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include "learner/QuickFoilTimer.hpp"

namespace quickfoil {

const char* QuickFoilTimer::kStageNames[] = {
    "generate_candidate_literals",
    "group_literals",
    "generate_plan",
    "evaluate_literals",
    "partition_background",
    "partition_and_build_bindings",
    "assigner",
    "hash_join",
    "filter",
    "count",
    "build_binding_table"};

static_assert(QuickFoilTimer::kNumberStages ==
              sizeof(QuickFoilTimer::kStageNames) / sizeof(QuickFoilTimer::kStageNames[0]),
              "The size of kStageNames is not equal to kNumberStages");

}

