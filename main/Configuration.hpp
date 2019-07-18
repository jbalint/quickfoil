/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#ifndef QUICKFOIL_MAIN_CONFIGURATION_HPP_
#define QUICKFOIL_MAIN_CONFIGURATION_HPP_

#include <string>
#include <utility>

#include "schema/TypeDefs.hpp"
#include "utility/Macros.hpp"
#include "utility/Vector.hpp"

#include "folly/dynamic.h"

namespace quickfoil {

struct PredicateArgumentConfiguration {
  PredicateArgumentConfiguration(int type_in,
                                 bool is_skipped_in)
      : type(type_in),
        is_skipped(is_skipped_in) {}

  int type;
  bool is_skipped;
};

struct PredicateConfiguration {
  PredicateConfiguration(const std::string& name_in,
                         const std::string& file_path_in,
                         Vector<PredicateArgumentConfiguration>&& arguments_in,
                         int key_in)
      : name(name_in),
        file_path(file_path_in),
        arguments(std::move(arguments_in)),
        key(key_in) {
  }

  std::string name;
  std::string file_path;
  Vector<PredicateArgumentConfiguration> arguments;
  int key;
};

struct TargetPredicateConfiguration {
  TargetPredicateConfiguration(const PredicateConfiguration predicate_configuration_in,
                               size_type num_positive_in)
      : predicate_configuration(predicate_configuration_in),
        num_positive(num_positive_in) {}

  PredicateConfiguration predicate_configuration;
  size_type num_positive;
};

struct TestSetting {
  TestSetting(const std::string test_file_path_in,
              size_type num_test_positive_in)
      : test_file_path(test_file_path_in),
        num_test_positive(num_test_positive_in) {
  }

  std::string test_file_path;
  size_type num_test_positive;
};

class Configuration {
 public:
  Configuration(const std::string& json_file_path);

  const TargetPredicateConfiguration& conf_for_target_predicate() const {
    return *conf_for_target_predicate_;
  }

  const Vector<PredicateConfiguration>& conf_for_background_predicates() const {
    return conf_for_background_predicates_;
  }

  const TestSetting* test_setting() const {
    return test_setting_.get();
  }

  static folly::dynamic ReadFromJson(const std::string& key,
                                     const folly::dynamic& json_node);

 private:
  std::unique_ptr<TargetPredicateConfiguration> conf_for_target_predicate_;
  Vector<PredicateConfiguration> conf_for_background_predicates_;
  std::unique_ptr<TestSetting> test_setting_;

  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

} /* namespace quickfoil */

#endif /* QUICKFOIL_MAIN_CONFIGURATION_HPP_ */
