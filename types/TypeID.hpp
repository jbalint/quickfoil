/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_TYPES_TYPE_ID_HPP_
#define QUICKFOIL_TYPES_TYPE_ID_HPP_

namespace quickfoil {

enum TypeID {
  kInt32,
  kInt64,
  kDouble,
  kString
};

static constexpr TypeID kQuickFoilDefaultDataType = kInt32;

}  // namespace quickfoil

#endif /* QUICKFOIL_TYPES_TYPE_ID_HPP_ */
