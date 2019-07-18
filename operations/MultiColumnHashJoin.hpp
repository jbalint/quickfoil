/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_MULTI_COLUMN_HASH_JOIN_HPP_
#define QUICKFOIL_OPERATIONS_MULTI_COLUMN_HASH_JOIN_HPP_

#include "expressions/AttributeReference.hpp"
#include "memory/Buffer.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "storage/FoilHashTable.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

void CreateLabelAwareBindingTables(const FoilClauseConstSharedPtr& clause,
                                   const FoilLiteral& new_literal,
                                   const size_type num_binding_positives,
                                   const size_type num_binding_negatives,
                                   Vector<ConstBufferPtr>* new_binding_table);

void CreateBindingTable(const FoilLiteral& new_literal,
                        const TableView& cur_binding_table,
                        Vector<ConstBufferPtr>* new_binding_table);

class MultiColumnHashJoin {
 public:
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  MultiColumnHashJoin(const TableView& probe_table,
                      const Vector<AttributeReference>& probe_keys,
                      Vector<AttributeReference>&& project_expressions)
      : probe_table_(probe_table),
        project_expressions_(std::move(project_expressions)) {
    const Vector<ConstBufferPtr>& probe_columns = probe_table_.columns();
    for (const AttributeReference& probe_key : probe_keys) {
      ConstBufferPtr probe_buffer;
      probe_key.Evaluate(probe_columns,
                         &probe_buffer);
      probe_key_values_.emplace_back(probe_buffer->as_type<cpp_type>());
    }
  }

  MultiColumnHashJoin(const TableView& probe_table,
                      const Vector<AttributeReference>& probe_keys,
                      const Vector<AttributeReference>& project_expressions)
      : probe_table_(probe_table),
        project_expressions_(project_expressions) {
    const Vector<ConstBufferPtr>& probe_columns = probe_table_.columns();
    for (const AttributeReference& probe_key : probe_keys) {
      ConstBufferPtr probe_buffer;
      probe_key.Evaluate(probe_columns,
                         &probe_buffer);
      probe_key_values_.emplace_back(probe_buffer->as_type<cpp_type>());
    }
  }

  template <bool resizeable, bool populate_probe_tids, bool populate_build_tids>
  void Join(const TableView& build_table,
            const FoilHashTable& hash_table,
            const Vector<AttributeReference>& build_keys,
            Vector<BufferPtr>* output_buffers);

  template <bool populate_probe_tids, bool populate_build_tids>
  void CollaborateJoin(const TableView& left_build_table,
                       const TableView& right_build_table,
                       const FoilHashTable& left_hash_table,
                       const FoilHashTable& right_hash_table,
                       const Vector<AttributeReference>& build_keys,
                       Vector<BufferPtr>* left_output_buffers,
                       Vector<BufferPtr>* right_output_buffers);

 private:
  template <bool resizeable, int num_keys, bool populate_probe_tids, bool populate_build_tids>
  void JoinImpl(const TableView& build_table,
                const FoilHashTable& hash_table,
                const Vector<const cpp_type*> build_values,
                Vector<BufferPtr>* output_buffers);

  template <int num_keys, bool populate_probe_tids, bool populate_build_tids>
  void CollaborateJoinImpl(const TableView& left_build_table,
                           const TableView& right_build_table,
                           const FoilHashTable& left_hash_table,
                           const FoilHashTable& right_hash_table,
                           const Vector<const cpp_type*>& left_build_key_values,
                           const Vector<const cpp_type*>& right_build_key_values,
                           Vector<BufferPtr>* left_output_buffers,
                           Vector<BufferPtr>* right_output_buffers);

  template <int num_keys, bool populate_probe_tids, bool populate_build_tids>
  void DoBlockJoin(
      const size_type num_probe_values,
      const Vector<const cpp_type*>& probe_values,
      const Vector<const cpp_type*>& build_values,
      const FoilHashTable& hash_table,
      Vector<size_type>* probe_tids,
      Vector<size_type>* build_tids);

  const TableView& probe_table_;
  Vector<const cpp_type*> probe_key_values_;
  Vector<AttributeReference> project_expressions_;

  DISALLOW_COPY_AND_ASSIGN(MultiColumnHashJoin);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_MULTI_COLUMN_HASH_JOIN_HPP_ */
