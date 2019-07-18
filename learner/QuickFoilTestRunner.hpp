/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_LEARNER_QUICK_FOIL_TEST_RUNNER_HPP_
#define QUICKFOIL_LEARNER_QUICK_FOIL_TEST_RUNNER_HPP_

#include <memory>
#include <utility>

#include "schema/TypeDefs.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class FoilClause;
class FoilLiteral;
class FoilPredicate;
class TableView;

class QuickFoilTestRunner {
 public:
  QuickFoilTestRunner(const FoilPredicate* target_predicate,
                      const Vector<std::unique_ptr<const FoilClause>>& clauses);

  ~QuickFoilTestRunner();

  size_type RunTest(const TableView& test_data) const;

 private:
  TableView* ComputeUnCoveredData(
      const TableView& current_uncovered_data,
      const TableView& binding_set,
      const FoilLiteral& literal) const;

  const FoilPredicate* target_predicate_;
  const Vector<std::unique_ptr<const FoilClause>>& clauses_;

  DISALLOW_COPY_AND_ASSIGN(QuickFoilTestRunner);
};

} /* namespace quickfoil */

#endif /* QUICKFOIL_LEARNER_QUICKFOILTESTRUNNER_HPP_ */
