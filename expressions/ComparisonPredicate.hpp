/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_EXPRESSIONS_COMPARISONPREDICATE_HPP_
#define QUICKFOIL_EXPRESSIONS_COMPARISONPREDICATE_HPP_

#include <memory>

#include "expressions/AttributeReference.hpp"
#include "expressions/OperatorTraits.hpp"
#include "schema/TypeDefs.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/BitVector.hpp"
#include "utility/BitVectorBuilder.hpp"
#include "utility/Macros.hpp"

namespace quickfoil {

template <OperatorType operator_type, TypeID type_id>
class ComparisonPredicate {
 public:
  typedef typename OperatorTraits<operator_type>::op op;
  typedef typename TypeTraits<type_id>::cpp_type cpp_type;

  ComparisonPredicate(AttributeReference* probe_attribute,
                      AttributeReference* build_attribute)
      : probe_attribute_(probe_attribute),
        build_attribute_(build_attribute) {}

  ComparisonPredicate(ComparisonPredicate&& other)
      : operator_(other.operator_),
        probe_attribute_(std::move(other.probe_attribute_)),
        build_attribute_(std::move(other.build_attribute_)) {}

  ComparisonPredicate(const ComparisonPredicate& other)
      : probe_attribute_(other.probe_attribute_->Clone()),
        build_attribute_(other.build_attribute_->Clone()) {}

  ComparisonPredicate* Clone() const {
    return new ComparisonPredicate(probe_attribute_->Clone(),
                                   build_attribute_->Clone());
  }

  void EvaluateForJoin(const Vector<ConstBufferPtr>& probe_columns,
                       const Vector<ConstBufferPtr>& build_columns,
                       const Vector<size_type>& probe_tids,
                       const Vector<size_type>& build_tids,
                       BitVector* output) {
    ConstBufferPtr probe_buffer;
    probe_attribute_->Evaluate(probe_columns, &probe_buffer);

    ConstBufferPtr build_buffer;
    build_attribute_->Evaluate(build_columns, &build_buffer);

    const cpp_type* __restrict__ probe_values =
        probe_buffer->as_type<cpp_type>();
    const cpp_type* __restrict__ build_values =
        build_buffer->as_type<cpp_type>();

    output->clear();
    output->resize(probe_tids.size());
    BitVectorBuilder result_builder(output);
    BitVectorBuilder::buffer_type::iterator raw_bit_vector_iterator =
        result_builder.bit_vector()->begin();

    Vector<size_type>::const_iterator probe_tids_it = probe_tids.begin();
    Vector<size_type>::const_iterator build_tids_it = build_tids.begin();
    for (std::size_t block_id = 0; block_id < result_builder.num_blocks(); ++block_id) {
      for (unsigned bit = 0; bit < 64; ++bit) {
        *raw_bit_vector_iterator |= (static_cast<BitVectorBuilder::block_type>(
            operator_(probe_values[*probe_tids_it], build_values[*build_tids_it])) << bit);
        ++probe_tids_it;
        ++build_tids_it;
      }
      ++raw_bit_vector_iterator;
    }

    for (unsigned bit = 0; bit < result_builder.bits_in_last_block(); ++bit) {
      *raw_bit_vector_iterator |= (static_cast<BitVectorBuilder::block_type>(
          operator_(probe_values[*probe_tids_it], build_values[*build_tids_it])) << bit);
      ++probe_tids_it;
      ++build_tids_it;
    }
  }

  const AttributeReference& probe_attribute() const {
    return *probe_attribute_;
  }

  const AttributeReference& build_attribute() const {
    return *build_attribute_;
  }

 private:
  op operator_;
  std::unique_ptr<AttributeReference> probe_attribute_;
  std::unique_ptr<AttributeReference> build_attribute_;
};

using FoilFilterPredicate = ComparisonPredicate<kEqual, kQuickFoilDefaultDataType>;

}

#endif /* QUICKFOIL_EXPRESSIONS_COMPARISONPREDICATE_HPP_ */
