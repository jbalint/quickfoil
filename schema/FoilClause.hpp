/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_SCHEMA_FOILCLAUSE_HPP_
#define QUICKFOIL_SCHEMA_FOILCLAUSE_HPP_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "memory/Buffer.hpp"
#include "schema/FoilVariable.hpp"
#include "schema/FoilLiteral.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class FoilPredicate;

class FoilClause;
typedef std::shared_ptr<const FoilClause> FoilClauseConstSharedPtr;

class FoilClause {
 public:
  typedef typename TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  // Takes ownership of head_literal.
  FoilClause(const FoilLiteral& head_literal)
      : head_literal_(head_literal),
        num_positive_bindings_(0),
        num_negative_bindings_(0),
        num_variables_without_last_body_literal_(head_literal.num_variables()) {
    for (const FoilVariable& variable : head_literal.variables()) {
      DCHECK(variable.IsBound());
      DCHECK_LE(variable.variable_id(), static_cast<int>(variables_.size()));
      if (variable.variable_id() == static_cast<int>(variables_.size())) {
        variables_.emplace_back(variable);
      }
    }
  }

  FoilClause(const FoilClause& clone)
      : head_literal_(clone.head_literal_),
        body_literals_(clone.body_literals_),
        variables_(clone.variables_),
        num_positive_bindings_(0),
        num_negative_bindings_(0),
        num_variables_without_last_body_literal_(clone.num_variables_without_last_body_literal_),
        random_flags_(clone.random_flags_) {
  }

  bool Equals(const FoilClause& other) const;

  inline const FoilLiteral& head_literal() const { return head_literal_; }

  inline const Vector<FoilLiteral>& body_literals() const {
    return body_literals_;
  }

  int num_body_literals() const {
    return body_literals_.size();
  }

  const Vector<ConstBufferPtr>& positive_blocks() const {
    DCHECK(!positive_blocks_.empty());
    return positive_blocks_;
  }

  Vector<ConstBufferPtr> CreatePositiveBlocks() const {
    DCHECK(!integral_blocks_.empty());
    Vector<ConstBufferPtr> positive_blocks;
    for (const ConstBufferPtr& block : integral_blocks_) {
      positive_blocks.emplace_back(
          std::make_shared<const ConstBuffer>(block,
                                              block->data(),
                                              num_positive_bindings_));
    }
    return positive_blocks;
  }

  Vector<ConstBufferPtr> CreateNegativeBlocks() const {
    DCHECK(!integral_blocks_.empty());
    Vector<ConstBufferPtr> negative_blocks;
    for (const ConstBufferPtr& block : integral_blocks_) {
      negative_blocks.emplace_back(
          std::make_shared<const ConstBuffer>(block,
                                              block->as_type<cpp_type>() + num_positive_bindings_,
                                              num_negative_bindings_));
    }
    return negative_blocks;
  }

  const Vector<ConstBufferPtr>& negative_blocks() const {
    DCHECK(!negative_blocks_.empty());
    return negative_blocks_;
  }

  const Vector<ConstBufferPtr>& integral_blocks() const {
    DCHECK(!integral_blocks_.empty());
    return integral_blocks_;
  }

  const Vector<FoilVariable>& variables() const {
    return variables_;
  }

  const FoilVariable& variable_at(int index) const {
    DCHECK_LT(index, static_cast<int>(variables_.size()));
    return variables_[index];
  }

  int num_variables() const {
    return variables_.size();
  }

  std::string ToString() const;

  // Checks whether all literals and variables are bound.
  bool IsBound() const;

  size_type GetNumTotalBindings() const {
    return num_positive_bindings_ + num_negative_bindings_;
  }

  size_type GetNumPositiveBindings() const {
    return num_positive_bindings_;
  }

  size_type GetNumNegativeBindings() const {
    return num_negative_bindings_;
  }

  static FoilClauseConstSharedPtr Create(const FoilLiteral& head_literal,
                                         const size_type num_positive_bindings,
                                         const size_type num_negative_bindings,
                                         const Vector<ConstBufferPtr>& binding_blocks) {
    std::shared_ptr<FoilClause> mutable_clause = std::make_shared<FoilClause>(head_literal);
    mutable_clause->num_positive_bindings_ = num_positive_bindings;
    mutable_clause->num_negative_bindings_ = num_negative_bindings;
    mutable_clause->integral_blocks_ = binding_blocks;
    return mutable_clause;
  }

  static FoilClauseConstSharedPtr Create(const FoilLiteral& head_literal,
                                         const Vector<ConstBufferPtr>& positive_blocks,
                                         const Vector<ConstBufferPtr>& negative_blocks) {
    std::shared_ptr<FoilClause> mutable_clause = std::make_shared<FoilClause>(head_literal);
    mutable_clause->num_positive_bindings_ = positive_blocks[0]->num_tuples();
    mutable_clause->num_negative_bindings_ = negative_blocks[0]->num_tuples();
    mutable_clause->positive_blocks_ = positive_blocks;
    mutable_clause->negative_blocks_ = negative_blocks;
    return mutable_clause;
  }

  FoilClauseConstSharedPtr CopyWithAdditionalUnBoundBodyLiteral(
      const FoilLiteral& new_body_literal,
      bool is_random,
      const size_type num_positive_bindings,
      const size_type num_negative_bindings,
      Vector<ConstBufferPtr>&& binding_blocks) const {
    std::shared_ptr<FoilClause> mutable_copy = std::make_shared<FoilClause>(*this);
    mutable_copy->AddUnBoundBodyLiteral(new_body_literal, is_random);
    mutable_copy->num_positive_bindings_ = num_positive_bindings;
    mutable_copy->num_negative_bindings_ = num_negative_bindings;
    mutable_copy->integral_blocks_ = std::move(binding_blocks);
    return mutable_copy;
  }

  bool IsBindingDataConseuctive() const {
    return !integral_blocks_.empty();
  }

  void AddUnBoundBodyLiteral(const FoilLiteral& body_literal, bool is_random);

  FoilClause* CopyWithoutData() const {
    return new FoilClause(*this);
  }

  inline int GetNumRandomLiterals() const {
    int num_random_literals = 0;
    for (const bool random_flag : random_flags_) {
      if (random_flag) {
        ++num_random_literals;
      }
    }
    return num_random_literals;
  }

  inline const Vector<bool>& random_flags() const {
    DCHECK_EQ(random_flags_.size(), body_literals_.size());
    return random_flags_;
  }

  inline int num_variables_without_last_body_literal() const {
    return num_variables_without_last_body_literal_;
  }

  FoilLiteral CreateUnboundLastLiteral() const {
    return body_literals_.back().CreateUnBoundLiteral(num_variables_without_last_body_literal_);
  }

 private:
  friend class FoilParser;

  void AddBoundBodyLiteral(const FoilLiteral& body_literal, bool is_random);

  FoilLiteral head_literal_;
  Vector<FoilLiteral> body_literals_;
  Vector<FoilVariable> variables_;  // 1:1 matching with binding_columns_;

  size_type num_positive_bindings_;
  size_type num_negative_bindings_;
  Vector<ConstBufferPtr> positive_blocks_;
  Vector<ConstBufferPtr> negative_blocks_;
  Vector<ConstBufferPtr> integral_blocks_;

  int num_variables_without_last_body_literal_;

  // 1:1 matching with body literals to indicate whether they are random.
  Vector<bool> random_flags_;

  void operator=(const FoilClause&) = delete;
};

} /* namespace quickfoil */

#endif /* QUICKFOIL_SCHEMA_FOILCLAUSE_HPP_ */
