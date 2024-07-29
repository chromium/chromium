// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/contiguous_memory_holder.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/memory/page_size.h"
#include "partition_alloc/page_allocator.h"

namespace memory_simulator {

ContiguousMemoryHolder::ContiguousMemoryHolder(size_t max_pages)
    : memory_length_(max_pages * base::GetPageSize()),
      memory_(partition_alloc::AllocPages(
          memory_length_,
          base::GetPageSize(),
          ::partition_alloc::PageAccessibilityConfiguration(
              partition_alloc::PageAccessibilityConfiguration::kReadWrite),
          partition_alloc::PageTag::kSimulation)),
      alloc_position_(reinterpret_cast<uint64_t*>(memory_)),
      read_position_(reinterpret_cast<uint64_t*>(memory_)),
      write_position_(reinterpret_cast<uint64_t*>(memory_)) {
  CHECK_NE(0u, memory_);
}

ContiguousMemoryHolder::~ContiguousMemoryHolder() {
  partition_alloc::FreePages(memory_, memory_length_);
}

void ContiguousMemoryHolder::Allocate() {
  DCHECK_EQ((reinterpret_cast<uintptr_t>(alloc_position_) - memory_) %
                base::GetPageSize(),
            0u);
  DCHECK_LT(reinterpret_cast<uintptr_t>(alloc_position_),
            memory_ + memory_length_);

  // Writing zeros to the memory page forces it to be added to this process'
  // private footprint.
  for (size_t i = 0; i < base::GetPageSize() / sizeof(uint64_t); ++i) {
    *alloc_position_ = 0;
    ++alloc_position_;
  }
}

void ContiguousMemoryHolder::Read() {
  if (read_position_ >= alloc_position_) {
    read_position_ = reinterpret_cast<uint64_t*>(memory_);
    if (read_position_ >= alloc_position_) {
      // No-op if the initial allocation didn't happen.
      return;
    }
  }

  DCHECK_LT(reinterpret_cast<uintptr_t>(read_position_),
            memory_ + memory_length_);
  DCHECK_EQ((reinterpret_cast<uintptr_t>(read_position_) - memory_) %
                base::GetPageSize(),
            0u);

  uint64_t sum = 0;
  for (size_t i = 0; i < base::GetPageSize() / sizeof(uint64_t); ++i) {
    sum += *read_position_;
    ++read_position_;
  }

  base::debug::Alias(&sum);
}

void ContiguousMemoryHolder::Write() {
  if (write_position_ >= alloc_position_) {
    write_position_ = reinterpret_cast<uint64_t*>(memory_);
    if (write_position_ >= alloc_position_) {
      // No-op if the initial allocation didn't happen.
      return;
    }
  }

  DCHECK_LT(reinterpret_cast<uintptr_t>(write_position_),
            memory_ + memory_length_);
  DCHECK_EQ((reinterpret_cast<uintptr_t>(write_position_) - memory_) %
                base::GetPageSize(),
            0u);

  for (size_t i = 0; i < base::GetPageSize() / sizeof(uint64_t); ++i) {
    *write_position_ = Rand();
    ++write_position_;
  }
}

}  // namespace memory_simulator
