/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "memory/Arena.hpp"

#include "utility/Vector.hpp"

#include "folly/Range.h"
#include "gtest/gtest.h"

namespace quickfoil {

TEST(ArenaTest, Basic) {
  Arena arena(10);
  Vector<folly::StringPiece> strs;
  for (int i = 0; i < 10000; ++i) {
    strs.emplace_back(arena.AddStringPiece(std::to_string(i*1.5)));
  }
  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(std::to_string(i*1.5), strs[i].data());
  }
}

}  // namespace quickfoil
