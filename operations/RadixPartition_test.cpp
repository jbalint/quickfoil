/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "operations/RadixPartition.hpp"

#include <set>

#include "memory/Buffer.hpp"
#include "storage/PartitionTuple.hpp"
#include "storage/TableView.hpp"
#include "types/Type.hpp"
#include "types/TypeID.hpp"
#include "types/TypeTraits.hpp"
#include "utility/Macros.hpp"
#include "utility/StringUtil.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

#define GENERATE_PARTITIONS_AND_CHECK(expected_partitions)                                   \
  do {                                                                                       \
    CreateTable();                                                                          \
    RadixPartition(0, table_.get());                                                           \
    const Vector<ConstBufferPtr>& partitions = table_->partitions_at(0);               \
    DCHECK_EQ(expected_partitions.size(), partitions.size());\
    for (size_t i = 0; i < partitions.size(); ++i) {\
      const RadixPartitionTest::multiset_type value_set = GenerateMultiset(partitions[i]);\
      EXPECT_TRUE(expected_partitions[i] == value_set) \
          << "Expected: " << ContainerToString(expected_partitions[i]) << "\n" \
          << "Actual: " << ContainerToString(value_set);\
    }\
  } while (false)

namespace quickfoil {

DECLARE_int32(num_radix_bits);

template <TypeID type_id>
struct PartitionTupleCompare {
  bool operator() (const PartitionTuple<type_id>& lhs,
                   const PartitionTuple<type_id>& rhs) const {
    return lhs.tuple_id < rhs.tuple_id;
  }
};

class RadixPartitionTest : public ::testing::Test {
 protected:
  typedef TypeTraits<kQuickFoilDefaultDataType>::cpp_type cpp_type;
  typedef std::multiset<PartitionTuple<kQuickFoilDefaultDataType>,
                        PartitionTupleCompare<kQuickFoilDefaultDataType>>  multiset_type;

  RadixPartitionTest() {}

  void SetColumnSize(int size) {
    block_ = std::make_shared<Buffer>(TypeTraits<kQuickFoilDefaultDataType>::size * size,
                                      size,
                                      nullptr);
    current_id_ = 0;
  }

  virtual ~RadixPartitionTest() {}

  void AddValue(cpp_type value) {
    block_->mutable_as_type<cpp_type>()[current_id_] = value;
    ++current_id_;
  }

  void CreateTable() {
    Vector<ConstBufferPtr> columns;
    columns.emplace_back(std::make_shared<const ConstBuffer>(block_));
    table_.reset(new TableView(std::move(columns)));
  }

  void Rewind() {
    current_id_ = 0;
  }

  multiset_type GenerateMultiset(const ConstBufferPtr& partition);

  BufferPtr block_;

  int current_id_ = -1;

  std::unique_ptr<TableView> table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RadixPartitionTest);
};

RadixPartitionTest::multiset_type RadixPartitionTest::GenerateMultiset(
    const ConstBufferPtr& partition) {
  multiset_type ret;
  const PartitionTuple<kQuickFoilDefaultDataType>* partition_tuples =
      partition->as_type<PartitionTuple<kQuickFoilDefaultDataType>>();
  for (std::size_t i = 0; i < partition->num_tuples(); ++i) {
    ret.emplace(partition_tuples->value, partition_tuples->tuple_id);
    ++partition_tuples;
  }
  return ret;
}

TEST_F(RadixPartitionTest, SimpleTests) {
  Vector<int> num_radix_bits_vec{1, 3, 7, 12};
  for (int num_radix_bits : num_radix_bits_vec) {
    FLAGS_num_radix_bits = num_radix_bits;
    const int partitions = 1 << num_radix_bits;
    Vector<int> test_sizes{10, 20, 40, 100, 1000, 100000};

    for (const int test_size : test_sizes) {
      LOG(INFO) << "Test_size=" << test_size << ", num_bits=" << FLAGS_num_radix_bits;
      SetColumnSize(test_size);
      Vector<RadixPartitionTest::multiset_type> expected_partitions(partitions);
      for (int i = 0; i < test_size; ++i) {
        AddValue(i);
        expected_partitions[i%partitions].emplace(i, i);
      }
      GENERATE_PARTITIONS_AND_CHECK(expected_partitions);
    }
  }
}

TEST_F(RadixPartitionTest, AllZero) {
  Vector<int> num_radix_bits_vec{1, 3, 7, 12};
  for (int num_radix_bits : num_radix_bits_vec) {
    FLAGS_num_radix_bits = num_radix_bits;
    const int partitions = 1 << num_radix_bits;

    Vector<int> test_sizes{10, 20, 40, 100, 1000, 100000};

    for (const int test_size : test_sizes) {
      LOG(INFO) << "Test_size=" << test_size << ", num_bits=" << FLAGS_num_radix_bits;
      SetColumnSize(test_size);
      Vector<RadixPartitionTest::multiset_type> expected_partitions(partitions);
      for (int i = 0; i < test_size; ++i) {
        AddValue(0);
        expected_partitions[0].emplace(0, i);
      }
      GENERATE_PARTITIONS_AND_CHECK(expected_partitions);
    }
  }
}

TEST_F(RadixPartitionTest, ZeroInterleaveOne) {
  Vector<int> num_radix_bits_vec{1, 3, 7, 12};
  for (int num_radix_bits : num_radix_bits_vec) {
    FLAGS_num_radix_bits = num_radix_bits;
    const int partitions = 1 << num_radix_bits;

    Vector<int> test_sizes{10, 20, 40, 100, 1000, 100000};

    for (const int test_size : test_sizes) {
      LOG(INFO) << "Test_size=" << test_size << ", num_bits=" << FLAGS_num_radix_bits;
      SetColumnSize(test_size);
      Vector<RadixPartitionTest::multiset_type> expected_partitions(partitions);
      for (int i = 0; i < test_size/2; ++i) {
        AddValue(0);
        AddValue(1);
        expected_partitions[0].emplace(0, 2*i);
        expected_partitions[1].emplace(1, 2*i + 1);
      }
      GENERATE_PARTITIONS_AND_CHECK(expected_partitions);
    }
  }
}

}  // namespace quickfoil

