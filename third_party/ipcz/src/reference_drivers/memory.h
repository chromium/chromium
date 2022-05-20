// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_MEMORY_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_MEMORY_H_

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::reference_drivers {

// Cross-platform abstraction for a shared memory region.
class Memory {
 public:
  // Cross-platform abstraction for an active mapping of a shared memory region.
  //
  // Instances of this object should be acquired from Memory::Map().
  class Mapping {
   public:
    Mapping();
    Mapping(void* base_address, size_t size);
    Mapping(Mapping&&);
    Mapping& operator=(Mapping&&);
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    ~Mapping();

    bool is_valid() const { return base_address_ != nullptr; }

    size_t size() const { return size_; }
    void* base() const { return base_address_; }

    absl::Span<uint8_t> bytes() const {
      return {static_cast<uint8_t*>(base()), size_};
    }

    template <typename T>
    T* As() const {
      return static_cast<T*>(base());
    }

    void Reset();

   private:
    void* base_address_ = nullptr;
    size_t size_ = 0;
  };

  // Constructs an invalid Memory object which cannot be mapped.
  Memory();

  // Constructs a new Memory object over `handle`, an OSHandle which should have
  // been previously taken from some other valid Memory object. `size` must
  // correspond to the size of that original region.
  Memory(OSHandle handle, size_t size);

  // Constructs a new Memory object over a newly allocated shared memory region
  // of at least `size` bytes.
  explicit Memory(size_t size);

  Memory(Memory&&);
  Memory& operator=(Memory&&);
  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;
  ~Memory();

  size_t size() const { return size_; }
  bool is_valid() const { return handle_.is_valid(); }
  const OSHandle& handle() const { return handle_; }

  // Invalidates this Memory object and returns an OSHandle which can be used
  // later to reconstruct an equivalent Memory object, given the same size().
  OSHandle TakeHandle() { return std::move(handle_); }

  // Resets this object, closing its handle to the underlying region.
  void reset() { handle_.reset(); }

  // Returns a new Memory object with its own handle to the same underlying
  // region as `this`. Must only be called on a valid Memory object (i.e.
  // is_valid() must be true.)
  Memory Clone();

  // Maps the entire region owned by Memory and returns a Mapping for it. Must
  // only be called on a valid Memory object (i.e. is_valid() must be true.)
  Mapping Map();

 private:
  OSHandle handle_;
  size_t size_ = 0;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_MEMORY_H_
