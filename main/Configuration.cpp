/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "main/Configuration.hpp"

#include <string>
#include <unordered_set>

#include "utility/Vector.hpp"

#include "folly/FileUtil.h"
#include "folly/json.h"
#include "glog/logging.h"

namespace quickfoil {

Configuration::Configuration(const std::string& json_file_path) {
  std::string file_content;
  CHECK(folly::readFile(json_file_path.data(), file_content))
      << "Cannot read the configuration file " << json_file_path;

  auto json_content = folly::parseJson(folly::json::stripComments(file_content));
  CHECK(json_content != nullptr && json_content.isObject());

  auto target_predicate_name = ReadFromJson("target", json_content);
  CHECK(target_predicate_name != nullptr);
  std::string target_predicate_name_str(target_predicate_name.asString().c_str());

  auto background_predicate_names = ReadFromJson("background", json_content);
  std::unordered_set<std::string> background_predicate_name_set;
  std::unordered_set<std::string> inserted_predicate_name_set;
  for (const auto& background_predicate_name : background_predicate_names) {
    CHECK(background_predicate_name.isString());
    background_predicate_name_set.emplace(background_predicate_name.asString().c_str());
  }

  auto predicate_configurations = ReadFromJson("relations", json_content);
  CHECK(predicate_configurations != nullptr && predicate_configurations.isArray());

  // Read each predicate/relation.
  for (const auto& predicate_configuration : predicate_configurations) {
    auto predicate_name = ReadFromJson("name", predicate_configuration);
    auto file_path = ReadFromJson("file", predicate_configuration);
    const auto& arguments = ReadFromJson("attributes", predicate_configuration);

    int key_pos = -1;
    auto key = ReadFromJson("key", predicate_configuration);
    if (key != nullptr) {
      key_pos = key.asInt();
    }

    CHECK(predicate_name != nullptr && predicate_name.isString())
        << "Predicate name must be a string";
    CHECK(file_path != nullptr && file_path.isString())
        << predicate_name << ": File path must be a string";
    CHECK(arguments != nullptr && arguments.isArray())
        << predicate_name<< ": The attributes/arguments must be in an array";

    CHECK (inserted_predicate_name_set.find(predicate_name.asString().c_str()) ==
        inserted_predicate_name_set.end())
        << "Duplicate predicates: " << predicate_name.asString();

    // Read each argument/attribute.
    Vector<PredicateArgumentConfiguration> argument_confs;
    for (const auto& argument : arguments) {
      auto argument_type = ReadFromJson("domain_type", argument);
      auto skip = ReadFromJson("skip", argument);

      CHECK(argument_type != nullptr && argument_type.isNumber());
      if (skip == nullptr || !skip.asBool()) {
        argument_confs.emplace_back(argument_type.asInt(), false);
      } else {
        argument_confs.emplace_back(argument_type.asInt(), true);
      }
    }

    PredicateConfiguration predicate_conf(predicate_name.asString().c_str(),
                                          file_path.asString().c_str(),
                                          std::move(argument_confs),
                                          key_pos);

    if (background_predicate_name_set.find(predicate_name.asString().c_str()) !=
        background_predicate_name_set.end()) {
      conf_for_background_predicates_.emplace_back(predicate_conf);
    } else if (target_predicate_name_str == predicate_name.asString().c_str()) {
      auto num_positive = ReadFromJson("num_positive", predicate_configuration);
      CHECK(num_positive != nullptr && num_positive.isNumber())
          << "num_positive must be a number";
      CHECK(num_positive.asInt() > 0)
          << "The number of positive tuples must be positive";
      conf_for_target_predicate_.reset(new TargetPredicateConfiguration(predicate_conf,
                                                                        num_positive.asInt()));
    }

    inserted_predicate_name_set.emplace(predicate_name.asString().c_str());
  }

  auto test = ReadFromJson("test", json_content);
  if (test != nullptr) {
    auto file_path = ReadFromJson("file", test);
    const std::string test_file_path = file_path.asString().c_str();
    auto num_positive = ReadFromJson("num_positive", test);
    CHECK(num_positive != nullptr && num_positive.isNumber())
        << "num_positive must be a number";
    CHECK(num_positive.asInt() > 0)
        << "The number of positive tuples must be positive";

    const size_type num_test_positive = num_positive.asInt();

    test_setting_.reset(new TestSetting(test_file_path,
                                        num_test_positive));
  }

  CHECK(conf_for_target_predicate_ != nullptr);
}

folly::dynamic Configuration::ReadFromJson(const std::string& key,
                                           const folly::dynamic& json_node) {
  auto found = json_node.find(key);
  if (found != json_node.items().end()) {
    return json_node[key];
  } else {
    return nullptr;
  }
}

} /* namespace quickfoil */
