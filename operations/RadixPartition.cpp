/*
 * This file copyright (c) 2014.
 * All rights reserved.
 */

#include "operations/RadixPartition.hpp"

#include <cstdlib>
#include <memory>

#include "memory/Buffer.hpp"
#include "memory/MemUtil.hpp"
#include "storage/PartitionTuple.hpp"
#include "storage/TableView.hpp"
#include "types/TypeID.hpp"
#include "utility/Hash.hpp"
#include "utility/Macros.hpp"
#include "utility/Math.hpp"
#include "utility/Vector.hpp"

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace quickfoil {

DEFINE_int32(num_radix_bits, 5, "Number of radix bits");

namespace {

class FreeDeleter {
 public:
  FreeDeleter(void* ptr) : ptr_(ptr) {
  }

  ~FreeDeleter() {
    free(ptr_);
  }

  void Release() {
    ptr_ = nullptr;
  }

 private:
  void* ptr_;
  DISALLOW_COPY_AND_ASSIGN(FreeDeleter);
};

template <TypeID type_id>
class RadixPartitioner {
 public:
  typedef typename TypeTraits<type_id>::cpp_type cpp_type;
  typedef PartitionTuple<type_id> partition_tuple_type;
  static constexpr const int block_byte_size = LCM(sizeof(partition_tuple_type), CACHE_LINE_SIZE);
  static constexpr const int block_capacity = block_byte_size / sizeof(partition_tuple_type);

  static void Partition(const ConstBufferPtr& block,
                        Vector<ConstBufferPtr>* partitions) {

    const uint32_t num_partitions =  1 << FLAGS_num_radix_bits;
    const uint32_t mask = num_partitions - 1;

    uint32_t* histogram = static_cast<uint32_t*>(calloc(num_partitions, sizeof(uint32_t)));
    FreeDeleter histogram_deleter(histogram);

    // Build a histogram.
    const std::uint32_t total_num_tuples = block->num_tuples();
    {
      const cpp_type* values = block->as_type<cpp_type>();
      for (std::uint32_t index = 0; index < total_num_tuples; ++index) {
        const hash_type hash_value = Hash(*values);
        ++histogram[HASH_BIT_MODULO(hash_value, mask, 0)];
        ++values;
      }
    }

    // Write buffer.
    CacheLine* write_buffer =
        static_cast<CacheLine*>(cacheline_aligned_alloc(num_partitions * sizeof(CacheLine)));
    DCHECK(write_buffer != nullptr);
    FreeDeleter write_buffer_deleter(write_buffer);

    // Output destination.
    size_type num_cache_lines =
        std::ceil(static_cast<double>(total_num_tuples) / block_capacity);
    const std::size_t output_data_size = sizeof(CacheLine) * num_cache_lines;
    BufferPtr output_buffer(new Buffer(
        output_data_size,
        total_num_tuples));
    CacheLine* __restrict__ output_destination =
        output_buffer->mutable_as_type<CacheLine>();

    std::vector<std::uint32_t> original_tuple_slots;
    original_tuple_slots.reserve(num_partitions);

    std::vector<std::uint8_t> original_buffer_slots;
    original_buffer_slots.reserve(num_partitions);

    size_type partition_offsets = 0;
    for (std::size_t i = 0; i < num_partitions; ++i) {
      write_buffer[i].partition_info.tuple_slot =
          partition_offsets / block_capacity;
      write_buffer[i].partition_info.buffer_slot =
          partition_offsets % block_capacity;
      original_tuple_slots.emplace_back(write_buffer[i].partition_info.tuple_slot);
      original_buffer_slots.emplace_back(write_buffer[i].partition_info.buffer_slot);
      partitions->emplace_back(
          std::make_shared<const ConstBuffer>(
              output_buffer,
              output_buffer->as_type<partition_tuple_type>() + partition_offsets,
              histogram[i]));
      partition_offsets += histogram[i];
    }

    const cpp_type* __restrict__ values = block->as_type<cpp_type>();
    for (std::size_t i = 0; i < total_num_tuples; ++i) {
      const hash_type hash_value = Hash(*values);
      CacheLine* __restrict__ write_buffer_entry =
          write_buffer + (hash_value & mask);
      const uint8_t buffer_destination_idx =
          write_buffer_entry->partition_info.buffer_slot;
      if (buffer_destination_idx == block_capacity - 1) {
        const uint32_t tuple_slot = write_buffer_entry->partition_info.tuple_slot;
        write_buffer_entry->partition_tuples.tuples[buffer_destination_idx].value = *values;
        write_buffer_entry->partition_tuples.tuples[buffer_destination_idx].tuple_id = i;
        cacheline_memcpy((output_destination + tuple_slot), write_buffer_entry);
        write_buffer_entry->partition_info.tuple_slot = tuple_slot + 1;
        write_buffer_entry->partition_info.buffer_slot = 0;
      } else {
        write_buffer_entry->partition_tuples.tuples[buffer_destination_idx].value = *values;
        write_buffer_entry->partition_tuples.tuples[buffer_destination_idx].tuple_id = i;
        ++write_buffer_entry->partition_info.buffer_slot;
      }
      ++values;
    }

    // Write left data in the buffers.
    for (std::size_t partition_id = 0; partition_id < num_partitions; ++partition_id) {
      const std::size_t current_tuple_slot = write_buffer[partition_id].partition_info.tuple_slot;

      if (original_tuple_slots[partition_id] != current_tuple_slot) {
        // We have written at least one cache line.
        const uint8_t num_buffer_slots_left = write_buffer[partition_id].partition_info.buffer_slot;
        memcpy(output_destination + current_tuple_slot,
               write_buffer[partition_id].partition_tuples.tuples,
               num_buffer_slots_left * sizeof(partition_tuple_type));
      } else {
        const uint8_t num_buffer_slots_filled =
            write_buffer[partition_id].partition_info.buffer_slot -
            original_buffer_slots[partition_id];
        memcpy(output_destination[current_tuple_slot].partition_tuples.tuples + original_buffer_slots[partition_id],
               write_buffer[partition_id].partition_tuples.tuples + original_buffer_slots[partition_id],
               num_buffer_slots_filled * sizeof(partition_tuple_type));
      }
    }
  }

 private:
  union CacheLine {
    struct partition_tuples {
      PartitionTuple<type_id> tuples[block_capacity];
    } partition_tuples;

    struct partition_info {
      PartitionTuple<type_id> tuples_[block_capacity - 1];

      uint32_t tuple_slot;
      uint8_t buffer_slot;
    } partition_info;
  } __attribute__ ((aligned(CACHE_LINE_SIZE)));

  static_assert(sizeof(CacheLine) == block_byte_size,
                "The size of PartitionBlock is not expected");

  DISALLOW_COPY_AND_ASSIGN(RadixPartitioner);
};

}  // namespace

void RadixPartition(int column_id,
                    TableView* table) {
  DCHECK_GT(FLAGS_num_radix_bits, 0);
  DCHECK(table->partitions_at(column_id).empty());
  Vector<ConstBufferPtr> partitions;
  RadixPartitioner<kQuickFoilDefaultDataType>::Partition(table->column_at(column_id), &partitions);
  table->set_partitions_at(column_id, std::move(partitions));
}

}  // namespace quickfoil
