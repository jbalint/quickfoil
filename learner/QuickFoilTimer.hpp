/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_QUICK_FOIL_TIMER_HPP_
#define QUICKFOIL_LEARNER_QUICK_FOIL_TIMER_HPP_

#include <chrono>

#include "utility/Macros.hpp"

#ifdef ENABLE_TIMING
#define START_TIMER(stage) QuickFoilTimer::GetInstance()->StartTimer(stage)
#define STOP_TIMER(stage) QuickFoilTimer::GetInstance()->StopTimer(stage)
#else
#define START_TIMER(stage)
#define STOP_TIMER(stage)
#endif

namespace quickfoil {

class QuickFoilTimer {
 public:
  enum Stage {
    kGenerateCandidateLiterals = 0,
    kGroupLiterals,
    kGeneratePlans,
    kEvaluateLiterals,
    kPartitionBackground,
    kPartitionAndBuildBindings,
    kAssigner,
    kHashJoin,
    kFilter,
    kCount,
    kCreateBindingTable,
    kNumberStages
  };

  static QuickFoilTimer* GetInstance() {
    static QuickFoilTimer timer;
    return &timer;
  }

  QuickFoilTimer() {
    for (int i = 0; i < kNumberStages; ++i) {
      elapsed_time_vec[i] = 0;
    }
  }

  inline void StartTimer(Stage stage) {
    start_time_vec[stage] = std::chrono::system_clock::now();
  }

  inline void StopTimer(Stage stage) {
    const std::chrono::duration<double> elapsed_seconds =
        std::chrono::system_clock::now() - start_time_vec[stage];
    elapsed_time_vec[stage] += elapsed_seconds.count();
  }

  double elapsed_time(int stage) const {
    return elapsed_time_vec[stage];
  }

  inline int num_stages() const {
    return kNumberStages;
  }

  static const char* kStageNames[];

 private:
  double elapsed_time_vec[kNumberStages];

  std::chrono::time_point<std::chrono::system_clock> start_time_vec[kNumberStages];

  DISALLOW_COPY_AND_ASSIGN(QuickFoilTimer);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_LEARNER_QUICK_FOIL_TIMER_HPP_ */
