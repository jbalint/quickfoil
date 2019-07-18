/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_SCHEMA_FOILPARSER_HPP_
#define QUICKFOIL_SCHEMA_FOILPARSER_HPP_

#include <cerrno>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "schema/FoilClause.hpp"
#include "schema/FoilLiteral.hpp"
#include "schema/FoilPredicate.hpp"
#include "schema/FoilVariable.hpp"
#include "utility/Macros.hpp"
#include "utility/StringUtil.hpp"
#include "utility/Vector.hpp"

#include "glog/logging.h"

namespace quickfoil {

class FoilParser {
 public:
  static int ReadInt(const std::string str);

  static FoilLiteral CreateLiteralFromString(
      const std::unordered_map<std::string, const FoilPredicate*>& name_to_predicate_map,
      const std::string& str);

  static FoilClause* CreateClauseFromString(
      const std::unordered_map<std::string, const FoilPredicate*>& name_to_predicate_map,
      const std::string& str);

  DISALLOW_COPY_AND_ASSIGN(FoilParser);
};

int FoilParser::ReadInt(const std::string str) {
  char* end;
  const int id = std::strtol(str.c_str(), &end, 0);
  DCHECK((errno != ERANGE) && end != str.c_str() && end != nullptr) << str;
  return id;
}

FoilLiteral FoilParser::CreateLiteralFromString(
      const std::unordered_map<std::string, const FoilPredicate*>& name_to_predicate_map,
      const std::string& str) {
  std::string processed_str = str;
  RemoveWhiteSpace(&processed_str);
  DCHECK(!processed_str.empty());

  const std::string::size_type name_end_pos = processed_str.find('(');
  DCHECK_GT(name_end_pos, 0);
  const std::string name = processed_str.substr(0, name_end_pos);

  const std::unordered_map<std::string, const FoilPredicate*>::const_iterator predicate_it =
      name_to_predicate_map.find(name);
  DCHECK(predicate_it != name_to_predicate_map.end())
      << "predicate name: " << name
      << "; literal string: " << processed_str;

  FoilLiteral literal(predicate_it->second);
  std::string::size_type next_search_pos = name_end_pos + 1;
  for (;;) {
    std::string::size_type comma_position = processed_str.find(',', next_search_pos);
    if (comma_position == std::string::npos) {
      break;
    }
    const std::string variable_id_str = processed_str.substr(next_search_pos, comma_position - next_search_pos);
    literal.AddVariable(FoilVariable(ReadInt(variable_id_str),
                                     predicate_it->second->argument_type_at(literal.num_variables())));
    next_search_pos = comma_position + 1;
  }

  const std::string::size_type literal_end_pos = processed_str.find(')', next_search_pos);
  DCHECK_GT(literal_end_pos, 0);
  literal.AddVariable(FoilVariable(ReadInt(processed_str.substr(next_search_pos, literal_end_pos - next_search_pos)),
                                   predicate_it->second->argument_type_at(literal.num_variables())));

  return literal;
}

FoilClause* FoilParser::CreateClauseFromString(
      const std::unordered_map<std::string, const FoilPredicate*>& name_to_predicate_map,
      const std::string& str) {
  const std::string::size_type head_end_pos = str.find(':');
  DCHECK_GT(head_end_pos, 0) << str;
  DCHECK_LT(head_end_pos, str.size() - 1) << str;
  DCHECK_EQ(str[head_end_pos + 1], '-') << str;
  const std::string head_literal_str = str.substr(0, head_end_pos);
  const FoilLiteral head_literal = CreateLiteralFromString(name_to_predicate_map,
                                                           head_literal_str);
  std::unique_ptr<FoilClause> clause(
      new FoilClause(head_literal));

  std::string::size_type literal_start_pos = head_end_pos + 2;
  std::string::size_type next_end_pos = str.find(')', literal_start_pos);
  while (next_end_pos != std::string::npos) {
    clause->AddBoundBodyLiteral(
        CreateLiteralFromString(name_to_predicate_map,
                                str.substr(literal_start_pos,
                                           next_end_pos - literal_start_pos + 1)),
        false);
    literal_start_pos = str.find(',', next_end_pos + 1);
    if (literal_start_pos == std::string::npos) {
      break;
    }
    ++literal_start_pos;
    next_end_pos = str.find(')', literal_start_pos);
  }

  return clause.release();
}

}  // namespace quickfoil

#endif /* QUICKFOIL_SCHEMA_FOILPARSER_HPP_ */
