/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_MACROS_HPP_
#define QUICKFOIL_UTILITY_MACROS_HPP_

#include "glog/logging.h"

namespace quickfoil {

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete

#ifndef QLOG
#ifdef ENABLE_LOGGING
#define QLOG VLOG(1)
#else
#define QLOG DVLOG(1)
#endif
#endif

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_MACROS_HPP_ */
