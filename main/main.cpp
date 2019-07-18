/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "main/Configuration.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "memory/Buffer.hpp"
#include "learner/QuickFoil.hpp"
#include "learner/QuickFoilTestRunner.hpp"
#include "learner/QuickFoilTimer.hpp"
#include "schema/FoilClause.hpp"
#include "schema/FoilPredicate.hpp"
#include "schema/TypeDefs.hpp"
#include "storage/TableView.hpp"
#include "types/FromString.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/ElementDeleter.hpp"
#include "utility/Vector.hpp"

#include <boost/algorithm/string.hpp>
#include "gflags/gflags.h"
#include "glog/logging.h"

using quickfoil::Configuration;
using quickfoil::PredicateConfiguration;
using quickfoil::QuickFoil;
using quickfoil::Vector;

DEFINE_bool(run_tests, true, "Whether to run tests if available");

DEFINE_bool(quickfoil_also_output_to_err,
            false ,
            "Whether printting the result summary to the error stream");

namespace quickfoil {
namespace {

DEFINE_int32(inital_block__size, 327680, "Initial block size when loading data from files");

void LoadData(const PredicateConfiguration& conf,
              const std::string& file_path,
              Vector<ConstBufferPtr>* output_const_buffers) {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  Vector<BufferPtr> output_buffers;
  Vector<cpp_type*> output_destinations;
  const std::size_t inital_block_bytes = FLAGS_inital_block__size * sizeof(cpp_type);
  size_type capacity = FLAGS_inital_block__size;
  for (size_t i = 0; i < conf.arguments.size(); ++i) {
    if (!conf.arguments[i].is_skipped) {
      output_buffers.emplace_back(std::make_shared<Buffer>(inital_block_bytes,
                                                           capacity));
      output_destinations.emplace_back(output_buffers.back()->mutable_as_type<cpp_type>());
    }
  }

  VLOG(2) << "Read data from " << file_path;
  std::ifstream in(file_path);
  CHECK(in.is_open())<< file_path;

  std::string line;
  size_type num_lines = 0;
  while (std::getline(in, line)) {
    if (capacity < num_lines) {
      capacity *= 1.5;
      const std::size_t new_block_bytes = capacity * sizeof(cpp_type);
      for (int i = 0; i < static_cast<int>(output_buffers.size()); ++i) {
        output_buffers[i]->Realloc(new_block_bytes, capacity);
        output_destinations[i] = output_buffers[i]->mutable_as_type<cpp_type>() + num_lines;
      }
    }

    if (!line.empty() && line.at(0) != '#') {
      std::vector<std::string> column_value_strs;
      boost::split(column_value_strs, line, boost::is_any_of("|"));
      CHECK_EQ(conf.arguments.size(), column_value_strs.size()) << line;
      int column_id = 0;
      for (size_t i = 0; i < column_value_strs.size(); ++i) {
        if (!conf.arguments[i].is_skipped) {
          cpp_type value;
          FromString<kQuickFoilDefaultDataType>(column_value_strs[i], &value);
          *output_destinations[column_id] = value;
          ++output_destinations[column_id];
          ++column_id;
        }
      }
      ++num_lines;
    }
  }

  const std::size_t actual_block_byptes = num_lines * sizeof(cpp_type);
  for (BufferPtr& output_buffer : output_buffers) {
    output_buffer->Realloc(actual_block_byptes, num_lines);
    output_const_buffers->emplace_back(
        std::make_shared<const ConstBuffer>(output_buffer));
  }


  VLOG(2) << "Read " << num_lines << " rows from file " << file_path;
}

}  // namespace

FoilPredicate* CreateBackgroundPredicate(int id,
                                         const PredicateConfiguration& conf) {
  Vector<int> argument_types;

  Vector<ConstBufferPtr> blocks;
  LoadData(conf, conf.file_path, &blocks);

  int column_id = 0;
  for (size_t i = 0; i < conf.arguments.size(); ++i) {
    if (!conf.arguments[i].is_skipped) {
      argument_types.emplace_back(conf.arguments[i].type);
      ++column_id;
    }
  }

  return new FoilPredicate(id,
                           conf.name,
                           conf.key,
                           argument_types,
                           std::move(blocks));
}

FoilPredicate* CreateTargetPredicate(int id,
                                     const TargetPredicateConfiguration& conf) {
  Vector<int> argument_types;

  Vector<ConstBufferPtr> blocks;
  LoadData(conf.predicate_configuration,
           conf.predicate_configuration.file_path,
           &blocks);

  int column_id = 0;
  for (size_t i = 0; i < conf.predicate_configuration.arguments.size(); ++i) {
    if (!conf.predicate_configuration.arguments[i].is_skipped) {
      argument_types.emplace_back(conf.predicate_configuration.arguments[i].type);
      ++column_id;
    }
  }

  return new FoilPredicate(id,
                           conf.predicate_configuration.name,
                           conf.predicate_configuration.key,
                           argument_types,
                           std::move(blocks));
}

void CreateTestTableViews(const TestSetting& test_setting,
                          const PredicateConfiguration& target_predicate_conf,
                          std::unique_ptr<TableView>* positive_table_view,
                          std::unique_ptr<TableView>* negative_table_view) {
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;

  Vector<ConstBufferPtr> blocks;
  LoadData(target_predicate_conf,
           test_setting.test_file_path,
           &blocks);

  Vector<ConstBufferPtr> positive_blocks;
  Vector<ConstBufferPtr> negative_blocks;
  for (const ConstBufferPtr& block : blocks) {
    positive_blocks.emplace_back(
        std::make_shared<const ConstBuffer>(
            block,
            block->data(),
            test_setting.num_test_positive));
    negative_blocks.emplace_back(
        std::make_shared<const ConstBuffer>(
            block,
            block->as_type<cpp_type>() + test_setting.num_test_positive,
            block->num_tuples() - test_setting.num_test_positive));
  }

  positive_table_view->reset(new TableView(std::move(positive_blocks)));
  negative_table_view->reset(new TableView(std::move(negative_blocks)));
}

}  // namespace quickfoil

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true /* remove flags */);
  CHECK(argc > 1) << "Usage: ./quickfoil <configuration_file_name>.json";
  google::InitGoogleLogging(argv[0]);

#ifndef ENABLE_LOGGING
#ifdef NDEBUG
  if (FLAGS_v > 0) {
    LOG(ERROR)
        << "To use the verbose mode, "
        << " you must build QuickFOIL with the option ENABLE_LOGGING enabled (cmake -DENABLE_LOGGING=1).";
  }
#endif
#endif

  Configuration conf(argv[1]);

  Vector<const quickfoil::FoilPredicate*> background_predicates;
  quickfoil::ElementDeleter<const quickfoil::FoilPredicate> background_predicates_deleter(
      &background_predicates);
  int predicate_id = 0;
  for (const PredicateConfiguration& background_predicate_conf : conf.conf_for_background_predicates()) {
    background_predicates.emplace_back(quickfoil::CreateBackgroundPredicate(predicate_id,
                                                                            background_predicate_conf));
    ++predicate_id;
  }

  std::unique_ptr<quickfoil::FoilPredicate> target_predicate(
      quickfoil::CreateTargetPredicate(predicate_id, conf.conf_for_target_predicate()));

  std::chrono::time_point<std::chrono::system_clock> start, end;
  start = std::chrono::system_clock::now();

  const std::size_t num_training_positive = conf.conf_for_target_predicate().num_positive;
  const std::size_t num_training_negative =
      target_predicate->GetNumTotalFacts() - conf.conf_for_target_predicate().num_positive;

  QuickFoil quick_foil(num_training_positive,
                       num_training_negative,
                       target_predicate.get(),
                       background_predicates);
  quick_foil.Learn();

  end = std::chrono::system_clock::now();
  const std::chrono::duration<double> elapsed_seconds = end-start;
  std::cout << "Elapsed time: " << elapsed_seconds.count() << "s\n";

  std::string timer_info;
#ifdef ENABLE_TIMING
  const quickfoil::QuickFoilTimer& timer = *quickfoil::QuickFoilTimer::GetInstance();
  for (int i = 0; i < timer.num_stages(); ++i) {
    timer_info
        .append(quickfoil::QuickFoilTimer::kStageNames[i])
        .append("=")
        .append(std::to_string(timer.elapsed_time(i)))
        .append(", ");
  }
  timer_info.pop_back();
  timer_info.pop_back();
#endif

  const Vector<std::unique_ptr<const quickfoil::FoilClause>>& learnt_clauses =
      quick_foil.learnt_clauses();
  std::cout <<"#Clauses = " << learnt_clauses.size() << "\n";
  for (const std::unique_ptr<const quickfoil::FoilClause>& clause : learnt_clauses) {
    std::cout << clause->ToString() << "\n";
  }

  if (conf.test_setting() != nullptr && FLAGS_run_tests) {
    std::unique_ptr<quickfoil::TableView> positive_table_view;
    std::unique_ptr<quickfoil::TableView> negative_table_view;
    quickfoil::CreateTestTableViews(*conf.test_setting(),
                                    conf.conf_for_target_predicate().predicate_configuration,
                                    &positive_table_view,
                                    &negative_table_view);
    quickfoil::QuickFoilTestRunner test_runner(target_predicate.get(), quick_foil.learnt_clauses());

    const std::size_t num_test_positive = positive_table_view->num_tuples();
    const std::size_t num_test_negative = negative_table_view->num_tuples();

    std::cout << "Use positive test data ("<< positive_table_view->num_tuples() << ") ...\n";
    const quickfoil::size_type num_uncovered_positive = test_runner.RunTest(*positive_table_view);
    std::cout << "Use negative test data (" << negative_table_view->num_tuples() << ") ...\n";
    const quickfoil::size_type num_uncovered_negative = test_runner.RunTest(*negative_table_view);
    const quickfoil::size_type num_covered_positive = num_test_positive - num_uncovered_positive;
    const quickfoil::size_type num_covered_negative = num_test_negative - num_uncovered_negative;

    std::cout << "#covered_test_positive=" << num_covered_positive << ", "
              << "#covered_test_negative=" << num_covered_negative << ", "
              << "#total_positive=" << num_test_positive << ", "
              << "#total_negative=" << num_test_negative << ", "
              << "precision="
              << ((num_covered_positive == 0 && num_covered_negative == 0) ?
                   0 : (static_cast<double>(num_covered_positive)/(num_covered_positive + num_covered_negative)))
              << ", recall="
              << static_cast<double>(num_covered_positive) / conf.test_setting()->num_test_positive;
    if (!timer_info.empty()) {
      std::cout << ", " << timer_info;
    }
    std::cout << "\n";

    if (FLAGS_quickfoil_also_output_to_err) {
      std::cerr << num_training_positive << "\t"
                << num_training_negative << "\t"
                << elapsed_seconds.count() << "\t"
                << num_covered_positive << "\t"
                << num_covered_negative << "\t"
                << ((num_covered_positive == 0 && num_covered_negative == 0) ?
                    0 : (static_cast<double>(num_covered_positive)/(num_covered_positive + num_covered_negative))) << "\t"
                << static_cast<double>(num_covered_positive) / conf.test_setting()->num_test_positive;
      if (!timer_info.empty()) {
        std::cerr << "\t" << timer_info;
      }
      std::cerr << "\n";
    }
  } else if (FLAGS_quickfoil_also_output_to_err) {
    std::cerr << num_training_positive << "\t"
              << num_training_negative << "\t"
              << elapsed_seconds.count() << "\n";
  }
}
