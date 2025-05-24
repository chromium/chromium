// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_
#define UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_

#include <objbase.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/win/windows_types.h"

struct IA2TextSelection;
struct IUnknown;

namespace ui {

// RAII type for COM arrays.
// Example:
//   base::win::ScopedCoMemArray<LONG> columns;
//   get_selectedColumns(columns.Receive(), columns.ReceiveSize())
//   ...
//   return;  <-- memory released
template <typename T>
class COMPONENT_EXPORT(AX_PLATFORM) ScopedCoMemArray {
 public:
  ScopedCoMemArray() = default;

  // Constructs an instance from the contents of `data`.
  explicit ScopedCoMemArray(std::vector<T>&& data);

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
  const T* data() const { return mem_ptr_; }
  T* data() { return mem_ptr_; }

  base::span<const T> as_span() const {
    // SAFETY: mem_ptr_ and size_ originate from accessibility COM calls.
    return UNSAFE_BUFFERS(
        base::span(mem_ptr_, base::checked_cast<size_t>(size_)));
  }

  const T& operator[](std::size_t pos) const { return as_span()[pos]; }

  T** Receive() {
    DCHECK_EQ(mem_ptr_, nullptr);  // To catch memory leaks.
    return &mem_ptr_;
  }
  LONG* ReceiveSize() { return &size_; }

 private:
  // Releases resources held in the elements of the array.
  static void FreeContents(base::span<const T> contents) {}

  void Reset(T* ptr, LONG size) {
    FreeContents(as_span());
    ::CoTaskMemFree(std::exchange(mem_ptr_, ptr));
    size_ = size;
  }

  // RAW_PTR_EXCLUSION: #addr-of (address returned from a function, also points
  // to memory managed by the COM Allocator rather than partition_alloc).
  RAW_PTR_EXCLUSION T* mem_ptr_ = nullptr;
  LONG size_ = 0;
};

template <typename T>
ScopedCoMemArray<T>::ScopedCoMemArray(std::vector<T>&& data)
    : mem_ptr_(reinterpret_cast<T*>(::CoTaskMemAlloc(data.size() * sizeof(T)))),
      size_(base::checked_cast<LONG>(data.size())) {
  if (!mem_ptr_) {
    base::TerminateBecauseOutOfMemory(data.size() * sizeof(T));
  }
  // SAFETY: mem_ptr_ is sized based on the contents of `data`.
  std::ranges::move(
      data,
      UNSAFE_BUFFERS(base::span(mem_ptr_, base::checked_cast<size_t>(size_)))
          .begin());
  data.clear();
}

// Release the references to the two IAccessibleText pointers in each element.
template <>
void ScopedCoMemArray<IA2TextSelection>::FreeContents(
    base::span<const IA2TextSelection> contents);

// Release the reference to each IUnknown pointer in the array.
template <>
void ScopedCoMemArray<IUnknown*>::FreeContents(
    base::span<IUnknown* const> contents);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_IACCESSIBLE2_SCOPED_CO_MEM_ARRAY_H_
