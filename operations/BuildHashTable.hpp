/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_OPERATIONS_BUILD_HASH_TABLE_HPP_
#define QUICKFOIL_OPERATIONS_BUILD_HASH_TABLE_HPP_

#include "expressions/AttributeReference.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/FoilHashTable.hpp"
#include "utility/Vector.hpp"

namespace quickfoil {

class SemiJoin;
class TableView;

void BuildHashTableOnPartitions(
    const int column_id,
    TableView* table);

FoilHashTable* BuildHashTableOnTable(
    const Vector<AttributeReference>& build_keys,
    const TableView& table);

FoilHashTable* BuildHashTableAfterSemiJoin(
    const size_type num_build_tuples,
    int num_build_keys,
    SemiJoin* semi_join);

}  // namespace quickfoil

#endif /* QUICKFOIL_OPERATIONS_BUILD_HASH_TABLE_HPP_ */
