/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_EXPRESSIONS_ATTRIBUTEREFERENCE_HPP_
#define QUICKFOIL_EXPRESSIONS_ATTRIBUTEREFERENCE_HPP_

#include "memory/Buffer.hpp"
#include "schema/TypeDefs.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/BitVector.hpp"
#include "utility/BitVectorIterator.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class AttributeReference {
 public:
  typedef typename TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  AttributeReference(int column_id)
      : column_id_(column_id) {}

  AttributeReference(const AttributeReference& other)
      : column_id_(other.column_id_) {}

  AttributeReference* Clone() const {
    return new AttributeReference(column_id_);
  }

  int column_id() const {
    return column_id_;
  }

  void Evaluate(const Vector<ConstBufferPtr>& columns,
                ConstBufferPtr* output) const {
    DCHECK_LT(column_id_, static_cast<int>(columns.size()));
    *output = columns[column_id_];
  }

  void EvaluateWithFilter(const Vector<const cpp_type*>& input_columns,
                          const BitVector& filter,
                          const size_type num_output,
                          const size_type start_output_pos,
                          Buffer* output) const {
    DCHECK_GT(num_output, 0);
    BitVectorIterator filter_it(filter);
    cpp_type* __restrict__ output_values =
        output->mutable_as_type<cpp_type>() + start_output_pos;
    const cpp_type* __restrict__ input_values =
        input_columns[column_id_];

    *output_values = input_values[filter_it.GetFirst()];
    ++output_values;
    for (size_type index = 1; index < num_output; ++index) {
      *output_values = input_values[filter_it.FindNext()];
      ++output_values;
    }
  }

  void EvaluateForJoin(const Vector<const cpp_type*>& probe_column,
                       const Vector<const cpp_type*>& build_column,
                       const Vector<size_type>& probe_tids,
                       const Vector<size_type>& build_tids,
                       const size_type start_output_pos,
                       Buffer* output) const {
    if (column_id_ < static_cast<int>(probe_column.size())) {
      WriteToBuffer(probe_tids,
                    probe_column[column_id_],
                    output->mutable_as_type<cpp_type>() + start_output_pos);
    } else {
      WriteToBuffer(build_tids,
                    build_column[column_id_ - static_cast<int>(probe_column.size())],
                    output->mutable_as_type<cpp_type>() + start_output_pos);
    }
  }

 private:
  template <typename cpp_type>
  inline void WriteToBuffer(const Vector<size_type>& tids,
                            const cpp_type* __restrict__ input_values,
                            cpp_type* __restrict__ output_values) const {
    for (size_type tid : tids) {
      *output_values = input_values[tid];
      ++output_values;
    }
  }

  int column_id_;

  void operator=(const AttributeReference&);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_EXPRESSIONS_ATTRIBUTEREFERENCE_HPP_ */
