/*
 * This file copyright (c) 2015.
 * All rights reserved.
 */

#ifndef QUICKFOIL_MEMORY_BUFFER_HPP_
#define QUICKFOIL_MEMORY_BUFFER_HPP_

#include <cstddef>
#include <cstdlib>
#include <memory>

#include "memory/MemUtil.hpp"
#include "utility/Macros.hpp"

namespace quickfoil {

class ConstBuffer;
typedef std::shared_ptr<const ConstBuffer> ConstBufferPtr;

class Buffer;
typedef std::shared_ptr<Buffer> BufferPtr;

class Buffer {
 public:
  Buffer(const std::size_t num_bytes,
         std::size_t num_tuples)
      : data_(qf_malloc(num_bytes)),
        num_tuples_(num_tuples) {
  }

  Buffer(void* data,
         const std::size_t num_bytes,
         std::size_t num_tuples)
      : data_(data),
        num_tuples_(num_tuples) {
  }

    Buffer(
           const std::size_t num_bytes,
           std::size_t num_tuples,
           void* data)
            : data_(data),
              num_tuples_(num_tuples) {
    }

  Buffer(const std::shared_ptr<Buffer>& parent_buffer,
         void* data,
         std::size_t num_tuples)
      : data_(data),
        num_tuples_(num_tuples),
        parent_buffer_(parent_buffer) {}

  ~Buffer() {
    if (parent_buffer_ == nullptr) {
      free(data_);
    }
  }

  inline std::size_t num_tuples() const {
    return num_tuples_;
  }

  void Realloc(std::size_t new_size, std::size_t new_num_tuples) {
#ifdef QUICKFOIL_ENABLE_MEMORY_MONITOR
    if (new_num_tuples > 0) {
      data_ = qf_realloc(data_, new_size * num_tuples_ / new_num_tuples, new_size);
    }
#else
    data_ = qf_realloc(data_, new_size);
#endif

    num_tuples_ = new_num_tuples;
  }

  inline void* mutable_data() {
    return data_;
  }

  inline const void* data() const {
    return data_;
  }

  template <typename data_type>
  inline const data_type* as_type() const {
    return static_cast<const data_type*>(data_);
  }

  template <typename data_type>
  inline data_type* mutable_as_type() {
    return static_cast<data_type*>(data_);
  }

 private:
  friend class ConstBuffer;

  void* data_;
  std::size_t num_tuples_;

  std::shared_ptr<Buffer> parent_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

class ConstBuffer  {
 public:
  ConstBuffer()
      : data_(nullptr),
        num_tuples_(0) {}

  ConstBuffer(const std::shared_ptr<const Buffer>& parent_buffer)
      : data_(parent_buffer->data()),
        num_tuples_(parent_buffer->num_tuples()),
        parent_buffer_(parent_buffer) {
  }

  ConstBuffer(const ConstBufferPtr& parent_buffer,
              const void* data,
              std::size_t num_tuples)
      : data_(data),
        num_tuples_(num_tuples),
        parent_buffer_(parent_buffer->parent_buffer_) {}

  ConstBuffer(const std::shared_ptr<const Buffer>& parent_buffer,
              const void* data,
              std::size_t num_tuples)
      : data_(data),
        num_tuples_(num_tuples),
        parent_buffer_(parent_buffer) {}

  inline const void* data() const {
    return data_;
  }

  template <typename data_type>
  inline const data_type* as_type() const {
    return static_cast<const data_type*>(data_);
  }

  inline std::size_t num_tuples() const {
    return num_tuples_;
  }

  const std::shared_ptr<const Buffer>& parent_buffer() const {
    return parent_buffer_;
  }

 private:
  const void* data_;
  std::size_t num_tuples_;

  std::shared_ptr<const Buffer> parent_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ConstBuffer);
};

}  // namespace quickfoil

#endif /* QUICKFOIL_MEMORY_BUFFER_HPP_ */
