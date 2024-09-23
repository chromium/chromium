// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_
#define UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_

#include <objbase.h>

#include <cstddef>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/win/windows_types.h"

namespace ui {

// RAII type for COM arrays.
// Example:
//   base::win::ScopedCoMemArray<LONG> columns;
//   get_selectedColumns(columns.Receive(), columns.ReceiveSize())
//   ...
//   return;  <-- memory released
template <typename T>
class ScopedCoMemArray {
 public:
  ScopedCoMemArray() = default;

  ScopedCoMemArray(const ScopedCoMemArray&) = delete;
  ScopedCoMemArray& operator=(const ScopedCoMemArray&) = delete;

  ScopedCoMemArray(ScopedCoMemArray&& o)
      : mem_ptr_(std::exchange(o.mem_ptr_, nullptr)),
        size_(std::exchange(o.size_, 0)) {}

  ScopedCoMemArray& operator=(ScopedCoMemArray&& o) {
    if (&o != this)
      Reset(std::exchange(o.mem_ptr_, nullptr), std::exchange(o.size_, 0));
    return *this;
  }

  ~ScopedCoMemArray() { Reset(nullptr, 0); }

  LONG size() const { return size_; }

  const T& operator[](std::size_t pos) const {
    CHECK_LT(static_cast<LONG>(pos), size_);
    return this->mem_ptr_[pos];
  }

  class Iterator final {
   public:
    Iterator(const ScopedCoMemArray* array, LONG index)
        : array_(array), index_(index) {}
    ~Iterator() {}

    Iterator& operator++() {
      ++index_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp(*this);
      operator++();
      return tmp;
    }

    const T& operator*() const { return (*array_)[index_]; }

    friend constexpr bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.array_ == rhs.array_ && lhs.index_ == rhs.index_;
    }
    friend constexpr bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return !(lhs == rhs);
    }

   private:
    raw_ptr<const ScopedCoMemArray> array_ = nullptr;
    LONG index_ = 0;
  };

  Iterator begin() const { return {this, 0}; }
  Iterator end() const { return {this, size_}; }

  T** Receive() {
    DCHECK_EQ(mem_ptr_, nullptr);  // To catch memory leaks.
    return &mem_ptr_;
  }
  LONG* ReceiveSize() { return &size_; }

 private:
  void Reset(T* ptr, LONG size) {
    ::CoTaskMemFree(std::exchange(mem_ptr_, ptr));
    size_ = size;
  }

  // RAW_PTR_EXCLUSION: #addr-of (address returned from a function, also points
  // to memory managed by the COM Allocator rather than partition_alloc).
  RAW_PTR_EXCLUSION T* mem_ptr_ = nullptr;
  LONG size_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_
