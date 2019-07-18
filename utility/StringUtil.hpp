/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_UTILITY_STRING_UTIL_HPP_
#define QUICKFOIL_UTILITY_STRING_UTIL_HPP_

#include <sstream>
#include <string>

#include "utility/Vector.hpp"

namespace quickfoil {

void RemoveWhiteSpace(std::string* str);

std::string RemoveWhiteSpace(const std::string& str);

template <class T>
std::string ContainerToString(const T& vec) {
  std::ostringstream out;
  bool first = true;
  for (const auto& element : vec) {
    if (first) {
      first = false;
    } else {
      out << "; ";
    }
    out << element.ToString();
  }
  return out.str();
}

template <class T>
std::string VectorOfVectorToString(const Vector<Vector<T>>& vec) {
  std::ostringstream out;
  out << "[";
  bool first_row = true;
  for (const Vector<T>& row : vec) {
    if (first_row) {
      first_row = false;
    } else {
      out << "; ";
    }
    out << "(";
    bool first_column = true;
    for (T column : row) {
      if (first_column) {
        first_column = false;
      } else {
        out << ", ";
      }
      out << column;
    }
    out << ")";
  }
  out << "]";
  return out.str();
}

}  // namespace quickfoil

#endif /* QUICKFOIL_UTILITY_STRING_UTIL_HPP_ */
