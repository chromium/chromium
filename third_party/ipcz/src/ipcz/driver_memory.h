// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_DRIVER_MEMORY_H_
#define IPCZ_SRC_IPCZ_DRIVER_MEMORY_H_

#include <cstddef>

#include "ipcz/driver_memory_mapping.h"
#include "ipcz/driver_object.h"
#include "ipcz/ipcz.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;

// Scoped wrapper around a shared memory region allocated and manipulated
// through an ipcz driver.
class DriverMemory {
 public:
  DriverMemory();

  // Takes ownership of an existing driver memory object.
  explicit DriverMemory(DriverObject memory);

  // Asks the node to allocate a new driver shared memory region of at least
  // `num_bytes` in size.
  DriverMemory(Ref<Node> node, size_t num_bytes);

  DriverMemory(DriverMemory&& other);
  DriverMemory& operator=(DriverMemory&& other);

  ~DriverMemory();

  bool is_valid() const { return memory_.is_valid(); }
  size_t size() const { return size_; }

  DriverObject& driver_object() { return memory_; }

  DriverObject TakeDriverObject() { return std::move(memory_); }

  // Asks the driver to clone this memory object and return a new one which
  // references the same underlying memory region.
  DriverMemory Clone();

  // Asks the driver to map this memory object into the process's address space
  // and returns a scoper to control the mapping's lifetime.
  DriverMemoryMapping Map();

 private:
  DriverObject memory_;
  size_t size_ = 0;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_DRIVER_MEMORY_H_
