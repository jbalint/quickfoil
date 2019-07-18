/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#include <memory>

#include "utility/Vector.hpp"

namespace quickfoil {

class AttributeReference;
class FoilHashTable;
class SemiJoin;
class TableView;

SemiJoin* SelectAndCreateSemiJoin(
    const TableView& output_table,
    const TableView& other_table,
    std::unique_ptr<FoilHashTable>* output_hash_table,
    std::unique_ptr<FoilHashTable>* other_hash_table,
    const Vector<AttributeReference>& output_join_keys,
    const Vector<AttributeReference>& other_join_keys,
    const Vector<int>& project_column_ids);

SemiJoin* CreateSemiJoin(bool left_semijoin,
                         const TableView& probe_table,
                         const TableView& build_table,
                         const FoilHashTable& build_hash_table,
                         const Vector<AttributeReference>& probe_keys,
                         const Vector<AttributeReference>& build_keys,
                         const Vector<int>& project_column_ids);
}

