/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "learner/CandidateLiteralEnumerator.hpp"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "learner/LiteralSearchStats.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilParser.hpp"
#include "schema/FoilPredicate.hpp"
#include "utility/Macros.hpp"
#include "utility/StringUtil.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"
#include "gtest/gtest.h"

#define DUPLICATE_EXPECT_CHECK(literals)                             \
  do {                                                               \
    const FoilLiteral* duplicate = FindDuplicateLiteral(literals);   \
    EXPECT_TRUE(duplicate == nullptr)                                \
        << "Duplicate: " << duplicate->ToString()                    \
        << "\nVector: " << ContainerToString(literals);              \
  } while (false)

#define DUPLICATE_ABORT_CHECK(literals)                              \
  do {                                                               \
    const FoilLiteral* duplicate = FindDuplicateLiteral(literals);   \
    CHECK(duplicate == nullptr)                                      \
        << duplicate->ToString()                                     \
        << "\n" << ContainerToString(literals);                      \
  } while (false)

#define EQUAL_CHECK(expected_literals, actual_literals)                              \
  do {                                                                               \
    DUPLICATE_ABORT_CHECK(expected_literals);                                        \
    DUPLICATE_EXPECT_CHECK(actual_literals);                                         \
    EXPECT_EQ(expected_literals.size(), actual_literals.size());                     \
    EXPECT_TRUE(Contains(expected_literals, actual_literals)                         \
                && Contains(actual_literals, expected_literals))                     \
        << "Expected: " << ContainerToString(VectorToSet(expected_literals)) << "\n" \
        << "Actual: " << ContainerToString(VectorToSet(actual_literals));            \
  } while (false)

#define EXPECT_DUPLICATE(literal_str)                                          \
  do {                                                                         \
    const FoilLiteral literal =                                                \
        FoilParser::CreateLiteralFromString(predicate_catalog_, literal_str);  \
    EXPECT_TRUE(IsReplaceableDuplicateLiteralToClause(literal))                \
        << literal.ToString() << "\n" << clause_->ToString();                  \
  } while (false)

#define EXPECT_NOT_DUPLICATE(literal_str)                                      \
  do {                                                                         \
    const FoilLiteral literal =                                                \
        FoilParser::CreateLiteralFromString(predicate_catalog_, literal_str);  \
    EXPECT_FALSE(IsReplaceableDuplicateLiteralToClause(literal))               \
        << literal.ToString() << "\n" << clause_->ToString();                  \
  } while (false)

namespace quickfoil {
namespace {

struct LiteralComparator {
  inline bool operator()(const FoilLiteral& lhs, const FoilLiteral& rhs) const {
    if (lhs.predicate()->id() != rhs.predicate()->id()) {
      return lhs.predicate()->id() < rhs.predicate()->id();
    }

    for (int i = 0; i < lhs.num_variables(); ++i) {
      if (lhs.variable_at(i).variable_id() != rhs.variable_at(i).variable_id()) {
        return lhs.variable_at(i).variable_id() < rhs.variable_at(i).variable_id();
      }
    }

    return false;
  }
};

}  // namespace

class CandidateLiteralEnumeratorTest : public ::testing::Test {
 protected:
  CandidateLiteralEnumeratorTest() {}

  void SetupClauseFromString(const std::string& clause_str) {
    predicate_to_body_literals_map_.clear();
    clause_.reset(FoilParser::CreateClauseFromString(predicate_catalog_,
                                                     clause_str));
    for (const FoilLiteral& body_literal : clause_->body_literals()) {
      predicate_to_body_literals_map_[body_literal.predicate()->id()].push_back(&body_literal);
    }
  }

  void SetUp() override {
    unary_predicate_.reset(new FoilPredicate(0,
                                             GeneratePredicateName(0),
                                             {0},
                                             {nullptr}));
    binary_predicate_0_.reset(new FoilPredicate(1,
                                              GeneratePredicateName(1),
                                              {0, 1},
                                              {nullptr, nullptr}));
    ternary_predicate_0_.reset(new FoilPredicate(2,
                                               GeneratePredicateName(2),
                                               {1, 0, 2},
                                               {nullptr, nullptr, nullptr}));
    ternary_predicate_1_.reset(new FoilPredicate(3,
                                                 GeneratePredicateName(3),
                                                 {0, 0, 1},
                                                 {nullptr, nullptr, nullptr}));
    ternary_predicate_2_.reset(new FoilPredicate(4,
                                                 GeneratePredicateName(4),
                                                 {1, 0, 1},
                                                 {nullptr, nullptr, nullptr}));
    binary_predicate_1_.reset(new FoilPredicate(5,
                                               GeneratePredicateName(5),
                                               {0, 0},
                                               {nullptr, nullptr}));

    predicate_catalog_.clear();
    predicate_catalog_.emplace(GeneratePredicateName(0), unary_predicate_.get());
    predicate_catalog_.emplace(GeneratePredicateName(1), binary_predicate_0_.get());
    predicate_catalog_.emplace(GeneratePredicateName(5), binary_predicate_1_.get());
    predicate_catalog_.emplace(GeneratePredicateName(2), ternary_predicate_0_.get());
    predicate_catalog_.emplace(GeneratePredicateName(3), ternary_predicate_1_.get());
    predicate_catalog_.emplace(GeneratePredicateName(4), ternary_predicate_2_.get());

    background_predicates_ = {
        unary_predicate_.get(),
        binary_predicate_0_.get(),
        binary_predicate_1_.get(),
        ternary_predicate_0_.get(),
        ternary_predicate_1_.get(),
        ternary_predicate_2_.get()};
    enumerator_.reset(new CandidateLiteralEnumerator(background_predicates_));
  }

  Vector<FoilLiteral> GenerateLiteralsForPredicate(const std::unordered_map<int, Vector<FoilVariable>>& variable_type_to_variable_map,
                                                            const FoilPredicate* predicate) const {
    Vector<Vector<FoilVariable>> variables_per_argument;
    enumerator_->GenerateVariableVectorForPredicate(variable_type_to_variable_map,
                                                    predicate,
                                                    &variables_per_argument);

    Vector<FoilLiteral> literals;
    literals.emplace_back(predicate);
    enumerator_->GenerateCandidateLiterals(0, variables_per_argument, &literals);
    return literals;
  }

  bool IsReplaceableDuplicateLiteralToClause(const FoilLiteral& literal) {
    VLOG(3) << "Check " << literal.ToString();
    enumerator_->canonical_databases_.clear();
    return enumerator_->CheckReplaceableDuplicate(*clause_, literal, predicate_to_body_literals_map_);
  }

  static const FoilLiteral* FindDuplicateLiteral(const Vector<FoilLiteral>& literals);

  // Checks whether literals0  contains literals1;
  static bool Contains(const Vector<FoilLiteral>& literals0, const Vector<FoilLiteral>& literals1);

  static Vector<FoilLiteral> ConvertLiteralMapToVector(
      const std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>& candidate_literals_map);

  static Vector<FoilLiteral> ConvertLiteralPtrMapToPtrVector(
      const std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>& candidate_literals_map);

  Vector<FoilLiteral> GenerateLiteralsFromStrings(const std::vector<std::string>& strs) const;

  std::unordered_map<int, Vector<const FoilLiteral*>> GeneratePredicateToLiteralsMap() const;

  void RunEnumeratorWithClauseSequences(const Vector<std::string>& strs);

  static std::set<FoilLiteral, LiteralComparator> VectorToSet(const Vector<FoilLiteral>& vec) {
    return {vec.begin(), vec.end()};
  }

  void SearchInMap(
      const std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>& literal_map,
      const Vector<FoilLiteral>& literal_queries,
      std::unordered_set<const FoilLiteral*>* result_set) const {
    for (const FoilLiteral& literal_query : literal_queries) {
      for (const auto& literal_map_entry : literal_map) {
        for (const FoilLiteral& literal_in_map : literal_map_entry.second) {
          if (literal_in_map.Equals(literal_query)) {
            result_set->insert(&literal_in_map);
          }
        }
      }
    }
  }

  std::unique_ptr<FoilClause> clause_;
  std::unordered_map<int, Vector<const FoilLiteral*>> predicate_to_body_literals_map_;

  std::unique_ptr<FoilPredicate> unary_predicate_;
  std::unique_ptr<FoilPredicate> binary_predicate_0_;
  std::unique_ptr<FoilPredicate> binary_predicate_1_;
  std::unique_ptr<FoilPredicate> ternary_predicate_0_;
  std::unique_ptr<FoilPredicate> ternary_predicate_1_;
  std::unique_ptr<FoilPredicate> ternary_predicate_2_;

  std::unordered_map<std::string, const FoilPredicate*> predicate_catalog_;
  std::unique_ptr<CandidateLiteralEnumerator> enumerator_;

 private:
  std::string GeneratePredicateName(int id) const {
    std::string ret("p_");
    ret.append(std::to_string(id));
    return ret;
  }

  Vector<const FoilPredicate*> background_predicates_;

  int next_pred_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CandidateLiteralEnumeratorTest);
};

const FoilLiteral* CandidateLiteralEnumeratorTest::FindDuplicateLiteral(const Vector<FoilLiteral>& literals) {
  for (size_t i = 0; i < literals.size(); ++i) {
    for (size_t j = i + 1; j < literals.size(); ++j) {
      if (literals[i].Equals(literals[j])) {
        return &literals[i];
      }
    }
  }
  return nullptr;
}

bool CandidateLiteralEnumeratorTest::Contains(const Vector<FoilLiteral>& literals0, const Vector<FoilLiteral>& literals1) {
  for (const FoilLiteral& literal_to_check : literals1) {
    bool found = false;
    for (const FoilLiteral& literal : literals0) {
      if (literal_to_check.Equals(literal)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

Vector<FoilLiteral> CandidateLiteralEnumeratorTest::GenerateLiteralsFromStrings(const std::vector<std::string>& strs) const {
  Vector<FoilLiteral> literals;
  for (const std::string& str : strs) {
    literals.emplace_back(FoilParser::CreateLiteralFromString(predicate_catalog_, str));
  }
  return literals;
}

Vector<FoilLiteral> CandidateLiteralEnumeratorTest::ConvertLiteralMapToVector(
    const std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>& candidate_literals_map) {
  Vector<FoilLiteral> literal_vec;
  for (const auto& pred_literal_vec_pair : candidate_literals_map) {
    literal_vec.insert(literal_vec.end(),
                       pred_literal_vec_pair.second.begin(),
                       pred_literal_vec_pair.second.end());
  }
  return literal_vec;
}

Vector<FoilLiteral> CandidateLiteralEnumeratorTest::ConvertLiteralPtrMapToPtrVector(
    const std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>& candidate_literals_map) {
  Vector<FoilLiteral> literal_vec;
  for (const auto& pred_literal_vec_pair : candidate_literals_map) {
    for (const FoilLiteral* literal : pred_literal_vec_pair.second) {
      literal_vec.emplace_back(*literal);
    }
  }
  return literal_vec;
}

TEST_F(CandidateLiteralEnumeratorTest, FoilParser) {
  const std::string clause_str1 =
      "p_0(0) :- p_1(1, 0), p_2(0, 1, 2), p_0(3), p_1(3, 4)";
  std::unique_ptr<FoilClause> parsed_clause(FoilParser::CreateClauseFromString(predicate_catalog_,
                                                                               clause_str1));
  EXPECT_EQ(RemoveWhiteSpace(clause_str1),
            RemoveWhiteSpace(parsed_clause->ToString()));

  const std::string clause_str2 = "p_0(0) :-       ";
  parsed_clause.reset(FoilParser::CreateClauseFromString(predicate_catalog_,
                                                         clause_str2));
  EXPECT_EQ(RemoveWhiteSpace(clause_str2),
            RemoveWhiteSpace(parsed_clause->ToString()));
}

TEST_F(CandidateLiteralEnumeratorTest, GenerateAllCandidateLiterals) {
  std::unordered_map<int, Vector<FoilVariable>> variable_type_to_variable_map = {
      {0, Vector<FoilVariable>{FoilVariable(0, 0), FoilVariable(1, 0)}},
      {1, Vector<FoilVariable>{FoilVariable(2, 1), FoilVariable(3, 1)}},
      {2, Vector<FoilVariable>{FoilVariable(4, 2), FoilVariable(5, 2), FoilVariable(6, 2)}}};

  Vector<FoilLiteral> expected_literals = GenerateLiteralsFromStrings({"p_0(0)", "p_0(1)", "p_0(-1)"});
  Vector<FoilLiteral> actual_literals = GenerateLiteralsForPredicate(variable_type_to_variable_map, unary_predicate_.get());

  EQUAL_CHECK(expected_literals, actual_literals);

  expected_literals = GenerateLiteralsFromStrings({
    "p_2(2, 0, 4)", "p_2(2, 0, 5)", "p_2(2, 0, 6)", "p_2(2, 0, -1)",
    "p_2(2, 1, 4)", "p_2(2, 1, 5)", "p_2(2, 1, 6)", "p_2(2, 1, -1)",
    "p_2(2, -1, 4)", "p_2(2, -1, 5)", "p_2(2, -1, 6)", "p_2(2, -1, -1)",
    "p_2(3, 0, 4)", "p_2(3, 0, 5)", "p_2(3, 0, 6)", "p_2(3, 0, -1)",
    "p_2(3, 1, 4)", "p_2(3, 1, 5)", "p_2(3, 1, 6)", "p_2(3, 1, -1)",
    "p_2(3, -1, 4)", "p_2(3, -1, 5)", "p_2(3, -1, 6)", "p_2(3, -1, -1)",
    "p_2(-1, 0, 4)", "p_2(-1, 0, 5)", "p_2(-1, 0, 6)", "p_2(-1, 0, -1)",
    "p_2(-1, 1, 4)", "p_2(-1, 1, 5)", "p_2(-1, 1, 6)", "p_2(-1, 1, -1)",
    "p_2(-1, -1, 4)", "p_2(-1, -1, 5)", "p_2(-1, -1, 6)", "p_2(-1, -1, -1)"});

  actual_literals = GenerateLiteralsForPredicate(variable_type_to_variable_map,
                                                 ternary_predicate_0_.get());
  EQUAL_CHECK(expected_literals, actual_literals);
}

TEST_F(CandidateLiteralEnumeratorTest, ReplaceableDuplicates) {
  SetupClauseFromString("p_0(0) :- p_1(0, 1)");

  EXPECT_DUPLICATE("p_1(0, -1)");
  EXPECT_NOT_DUPLICATE("p_1(-1, 1)");

  SetupClauseFromString("p_0(0) :- p_1(0, 1), p_2(1, 0, 2)");
  EXPECT_NOT_DUPLICATE("p_2(1, -1, 2)");
  EXPECT_NOT_DUPLICATE("p_1(0, -1)");
  EXPECT_DUPLICATE("p_2(1, 0, -1)");

  SetupClauseFromString("p_5(0, 1) :- p_5(0, 2), p_5(2, 1), p_5(0, 3)");
  EXPECT_DUPLICATE("p_5(0, -1)");
  EXPECT_DUPLICATE("p_5(3, 1)");
  EXPECT_DUPLICATE("p_5(3, -1)");
  EXPECT_NOT_DUPLICATE("p_5(1, 2)");
}

TEST_F(CandidateLiteralEnumeratorTest, EnumeratorTestWithoutDynamicPruning) {
  Vector<std::string> clause_strs{
    "p_5(0, 1) :-",
    "p_5(0, 1) :- p_5(0, 2)",
    "p_5(0, 1) :- p_5(0, 2), p_5(2, 1)",
    "p_5(0, 1) :- p_5(0, 2), p_5(2, 1), p_5(0, 3)"
  };

  std::unique_ptr<LiteralSearchStats> literal_search_stats(new LiteralSearchStats);
  std::shared_ptr<std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>> entire_generated_literals;
  std::unique_ptr<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>> pruned_generated_literals;

  for (const std::string& clause_str : clause_strs) {
    clause_.reset(FoilParser::CreateClauseFromString(predicate_catalog_,
                                                     clause_str));
    entire_generated_literals.reset(new std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>);
    pruned_generated_literals.reset(new std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>);
    enumerator_->EnumerateCandidateLiterals(*clause_,
                                            *literal_search_stats,
                                            entire_generated_literals.get(),
                                            pruned_generated_literals.get());
    literal_search_stats.reset(new LiteralSearchStats(clause_->num_variables(),
                                                      entire_generated_literals,
                                                      new std::unordered_set<const FoilLiteral*>()));
  }

  Vector<FoilLiteral> actual_entired_literals =
      ConvertLiteralMapToVector(*entire_generated_literals);
  Vector<FoilLiteral> actual_pruned_literals =
      ConvertLiteralPtrMapToPtrVector(*pruned_generated_literals);
  Vector<FoilLiteral> expected_entire_literals =
      GenerateLiteralsFromStrings({"p_0(0)"," p_0(1)"," p_0(2)"," p_0(3)"," p_1(0, -1)"," p_1(1, -1)"," p_1(2, -1)"," p_1(3, -1)"," p_2(-1, 0, -1)"," p_2(-1, 1, -1)"," p_2(-1, 2, -1)"," p_2(-1, 3, -1)"," p_3(-1, 0, -1)"," p_3(-1, 1, -1)"," p_3(-1, 2, -1)"," p_3(-1, 3, -1)"," p_3(0, -1, -1)"," p_3(0, 0, -1)"," p_3(0, 1, -1)"," p_3(0, 2, -1)"," p_3(0, 3, -1)"," p_3(1, -1, -1)"," p_3(1, 0, -1)"," p_3(1, 1, -1)"," p_3(1, 2, -1)"," p_3(1, 3, -1)"," p_3(2, -1, -1)"," p_3(2, 0, -1)"," p_3(2, 1, -1)"," p_3(2, 2, -1)"," p_3(2, 3, -1)"," p_3(3, -1, -1)"," p_3(3, 0, -1)"," p_3(3, 1, -1)"," p_3(3, 2, -1)"," p_3(3, 3, -1)"," p_4(-1, 0, -1)"," p_4(-1, 1, -1)"," p_4(-1, 2, -1)"," p_4(-1, 3, -1)"," p_5(-1, 0)"," p_5(-1, 1)"," p_5(-1, 2)"," p_5(-1, 3)"," p_5(0, -1)"," p_5(0, 0)"," p_5(0, 2)"," p_5(0, 3)"," p_5(1, -1)"," p_5(1, 0)"," p_5(1, 1)"," p_5(1, 2)"," p_5(1, 3)"," p_5(2, -1)"," p_5(2, 0)"," p_5(2, 2)"," p_5(2, 3)"," p_5(3, -1)"," p_5(3, 0)"," p_5(3, 1)"," p_5(3, 2)"," p_5(3, 3)"});
  Vector<FoilLiteral> expected_pruned_literals =
      GenerateLiteralsFromStrings({"p_0(0)", " p_1(0, -1)", " p_2(-1, 0, -1)", " p_3(-1, 0, -1)", " p_3(0, -1, -1)", " p_3(0, 3, -1)", " p_3(0, 2, -1)", " p_3(0, 1, -1)", " p_3(0, 0, -1)", " p_4(-1, 0, -1)", " p_5(-1, 0)", " p_5(0, 0)", " p_0(1)", " p_1(1, -1)", " p_2(-1, 1, -1)", " p_3(-1, 1, -1)", " p_3(1, -1, -1)", " p_3(1, 3, -1)", " p_3(1, 2, -1)", " p_3(1, 1, -1)", " p_3(1, 0, -1)", " p_4(-1, 1, -1)", " p_5(1, -1)", " p_5(1, 3)", " p_5(1, 2)", " p_5(1, 1)", " p_5(1, 0)", " p_0(2)", " p_1(2, -1)", " p_2(-1, 2, -1)", " p_3(-1, 2, -1)", " p_3(2, -1, -1)", " p_3(2, 3, -1)", " p_3(2, 2, -1)", " p_3(2, 1, -1)", " p_3(2, 0, -1)", " p_4(-1, 2, -1)", " p_5(2, 2)", " p_5(2, 0)", " p_0(3)", " p_1(3, -1)", " p_2(-1, 3, -1)", " p_3(-1, 3, -1)", " p_3(3, -1, -1)", " p_3(3, 3, -1)", " p_3(3, 2, -1)", " p_3(3, 1, -1)", " p_3(3, 0, -1)", " p_4(-1, 3, -1)", "p_5(3, 0)"});

  EQUAL_CHECK(expected_entire_literals, actual_entired_literals);
  EQUAL_CHECK(expected_pruned_literals, actual_pruned_literals);
}

TEST_F(CandidateLiteralEnumeratorTest, EnumeratorTestWithDynamicPruning) {
  Vector<std::string> clause_strs{
    "p_5(0, 1) :-",
    "p_5(0, 1) :- p_5(0, 2)",
    "p_5(0, 1) :- p_5(0, 2), p_5(2, 1)",
    "p_5(0, 1) :- p_5(0, 2), p_5(2, 1), p_5(0, 3)"
  };

  std::unique_ptr<LiteralSearchStats> literal_search_stats(new LiteralSearchStats);
  std::shared_ptr<std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>> entire_generated_literals;
  std::unique_ptr<std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>> pruned_generated_literals;

  Vector<FoilLiteral> removed_literals_in_it0 =
      GenerateLiteralsFromStrings({"p_1(0, -1)", "p_3(-1, 0, -1)", "p_3(0, -1, -1)"});

  Vector<FoilLiteral> removed_literals_in_it1 =
      GenerateLiteralsFromStrings({"p_3(2, -1, -1)", "p_5(2, -1)"});

  int iteration = 0;
  for (const std::string& clause_str : clause_strs) {
    clause_.reset(FoilParser::CreateClauseFromString(predicate_catalog_,
                                                     clause_str));
    entire_generated_literals.reset(new std::unordered_map<const FoilPredicate*, Vector<FoilLiteral>>);
    pruned_generated_literals.reset(new std::unordered_map<const FoilPredicate*, Vector<const FoilLiteral*>>);
    enumerator_->EnumerateCandidateLiterals(*clause_,
                                            *literal_search_stats,
                                            entire_generated_literals.get(),
                                            pruned_generated_literals.get());

    std::unique_ptr<std::unordered_set<const FoilLiteral*>> literals_to_be_removed(
        new std::unordered_set<const FoilLiteral*>());

    if (iteration == 0) {
      SearchInMap(*entire_generated_literals,
                  removed_literals_in_it0,
                  literals_to_be_removed.get());
    } else if (iteration == 1) {
      SearchInMap(*entire_generated_literals,
                  removed_literals_in_it1,
                  literals_to_be_removed.get());
    }
    ++iteration;

    literal_search_stats.reset(new LiteralSearchStats(clause_->num_variables(),
                                                      entire_generated_literals,
                                                      literals_to_be_removed.release()));
  }

  Vector<FoilLiteral> actual_entired_literals =
      ConvertLiteralMapToVector(*entire_generated_literals);
  Vector<FoilLiteral> actual_pruned_literals =
      ConvertLiteralPtrMapToPtrVector(*pruned_generated_literals);
  Vector<FoilLiteral> expected_entire_literals =
      GenerateLiteralsFromStrings({"p_0(0)"," p_0(1)"," p_0(2)"," p_0(3)"," p_1(1, -1)"," p_1(2, -1)"," p_1(3, -1)"," p_2(-1, 0, -1)"," p_2(-1, 1, -1)"," p_2(-1, 2, -1)"," p_2(-1, 3, -1)"," p_3(-1, 1, -1)"," p_3(-1, 2, -1)"," p_3(-1, 3, -1)"," p_3(0, 0, -1)"," p_3(0, 1, -1)"," p_3(1, -1, -1)"," p_3(1, 0, -1)"," p_3(1, 1, -1)"," p_3(1, 2, -1)"," p_3(1, 3, -1)"," p_3(2, 1, -1)"," p_3(2, 2, -1)"," p_3(3, -1, -1)"," p_3(3, 1, -1)"," p_3(3, 2, -1)"," p_3(3, 3, -1)"," p_4(-1, 0, -1)"," p_4(-1, 1, -1)"," p_4(-1, 2, -1)"," p_4(-1, 3, -1)"," p_5(-1, 0)"," p_5(-1, 1)"," p_5(-1, 2)"," p_5(-1, 3)"," p_5(0, -1)"," p_5(0, 0)"," p_5(0, 2)"," p_5(0, 3)"," p_5(1, -1)"," p_5(1, 0)"," p_5(1, 1)"," p_5(1, 2)"," p_5(1, 3)"," p_5(2, 0)"," p_5(2, 2)"," p_5(3, -1)"," p_5(3, 0)"," p_5(3, 1)"," p_5(3, 2)"," p_5(3, 3)"});
  Vector<FoilLiteral> expected_pruned_literals =
      GenerateLiteralsFromStrings({"p_0(0)"," p_0(1)"," p_0(2)"," p_0(3)"," p_1(1, -1)"," p_1(2, -1)"," p_1(3, -1)"," p_2(-1, 0, -1)"," p_2(-1, 1, -1)"," p_2(-1, 2, -1)"," p_2(-1, 3, -1)"," p_3(-1, 1, -1)"," p_3(-1, 2, -1)"," p_3(-1, 3, -1)"," p_3(0, 0, -1)"," p_3(0, 1, -1)"," p_3(1, -1, -1)"," p_3(1, 0, -1)"," p_3(1, 1, -1)"," p_3(1, 2, -1)"," p_3(1, 3, -1)"," p_3(2, 1, -1)"," p_3(2, 2, -1)"," p_3(3, -1, -1)"," p_3(3, 1, -1)"," p_3(3, 2, -1)"," p_3(3, 3, -1)"," p_4(-1, 0, -1)"," p_4(-1, 1, -1)"," p_4(-1, 2, -1)"," p_4(-1, 3, -1)"," p_5(-1, 0)"," p_5(0, 0)"," p_5(1, -1)"," p_5(1, 0)"," p_5(1, 1)"," p_5(1, 2)"," p_5(1, 3)"," p_5(2, 0)"," p_5(2, 2)"," p_5(3, 0)"});

  EQUAL_CHECK(expected_entire_literals, actual_entired_literals);
  EQUAL_CHECK(expected_pruned_literals, actual_pruned_literals);
}

}  // namespace quickfoil
