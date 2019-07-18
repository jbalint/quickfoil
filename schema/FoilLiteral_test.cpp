/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "schema/FoilLiteral.hpp"

#include "schema/FoilParser.hpp"

#include "gtest/gtest.h"

namespace quickfoil {

TEST(FoilLiteralTest, FoilLiteralSetTest) {
  FoilPredicate pred1(1, "p1", 0, {0, 0, 1}, {nullptr, nullptr, nullptr});
  FoilPredicate pred2(2, "p2", 1, {1, 0, 1}, {nullptr, nullptr, nullptr});

  std::unordered_map<std::string, const FoilPredicate*> name_to_predicate_map
      {{"p1", &pred1}, {"p2", &pred2}};

  FoilLiteral literal1 = FoilParser::CreateLiteralFromString(name_to_predicate_map,
                                                             "p1(0, 0, 1)");
  FoilLiteral literal2 = FoilParser::CreateLiteralFromString(name_to_predicate_map,
                                                             "p1(0, 0, -1)");
  FoilLiteral literal3 = FoilParser::CreateLiteralFromString(name_to_predicate_map,
                                                             "p1(0, 0, 2)");
  FoilLiteral literal4 = FoilParser::CreateLiteralFromString(name_to_predicate_map,
                                                             "p1(-1, 0, -1)");

  FoilLiteral p2_literal1 = FoilParser::CreateLiteralFromString(name_to_predicate_map,
                                                                "p2(0, 0, 1)");

  FoilLiteralSet set{literal1, literal2, literal4};
  EXPECT_TRUE(set.find(literal1) != set.end());
  EXPECT_TRUE(set.find(p2_literal1) == set.end());
  EXPECT_TRUE(set.find(literal2) != set.end());
  EXPECT_FALSE(set.find(literal3) != set.end());
  EXPECT_TRUE(set.find(literal4) != set.end());

  EXPECT_TRUE(set.find(literal3.CreateUnBoundLiteral(1)) != set.end());
  EXPECT_TRUE(set.find(literal3.CreateUnBoundLiteral(2)) != set.end());
  EXPECT_TRUE(set.find(literal1.CreateUnBoundLiteral(2)) != set.end());
  EXPECT_FALSE(set.find(p2_literal1.CreateUnBoundLiteral(2)) != set.end());
  EXPECT_TRUE(set.find(literal4.CreateUnBoundLiteral(0)) == set.end());
}

}  // namespace quickfoil

