/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "utility/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace quickfoil {

void RemoveWhiteSpace(std::string* str) {
  str->erase(std::remove_if(str->begin(), str->end(), ::isspace), str->end());
}

std::string RemoveWhiteSpace(const std::string& str) {
  std::string str_without_space = str;
  RemoveWhiteSpace(&str_without_space);
  return str_without_space;
}

}  // namespace quickfoil
