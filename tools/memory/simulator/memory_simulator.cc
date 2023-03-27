// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/memory_simulator.h"

#include <cstdint>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/time/time.h"
#include "tools/memory/simulator/contiguous_memory_holder.h"

namespace memory_simulator {

MemorySimulator::MemorySimulator() = default;
MemorySimulator::~MemorySimulator() = default;

void MemorySimulator::Start(int64_t page_alloc_per_sec,
                            int64_t page_read_per_sec,
                            int64_t page_write_per_sec,
                            int64_t max_pages_allocated,
                            base::TimeTicks read_deadline,
                            base::TimeTicks write_deadline) {
  memory_holder_ =
      std::make_unique<ContiguousMemoryHolder>(max_pages_allocated);
  max_pages_allocated_ = max_pages_allocated;

  if (page_alloc_per_sec != 0) {
    alloc_timer_.Start(FROM_HERE, base::Hertz(page_alloc_per_sec), this,
                       &MemorySimulator::Allocate);
  }

  if (page_read_per_sec != 0) {
    read_timer_.Start(FROM_HERE, base::Hertz(page_read_per_sec), this,
                      &MemorySimulator::Read);
    read_deadline_ = read_deadline;
  }

  if (page_write_per_sec != 0) {
    write_timer_.Start(FROM_HERE, base::Hertz(page_write_per_sec), this,
                       &MemorySimulator::Write);
    write_deadline_ = write_deadline;
  }
}

void MemorySimulator::StopAndFree() {
  memory_holder_.reset();
  alloc_timer_.Stop();
  read_timer_.Stop();
  write_timer_.Stop();
  pages_allocated_ = 0;
}

int64_t MemorySimulator::GetPagesAllocated() const {
  return pages_allocated_;
}

int64_t MemorySimulator::GetPagesRead() const {
  return pages_read_;
}

int64_t MemorySimulator::GetPagesWritten() const {
  return pages_written_;
}

void MemorySimulator::Allocate() {
  CHECK_LE(pages_allocated_, max_pages_allocated_);
  if (pages_allocated_ == max_pages_allocated_) {
    alloc_timer_.Stop();
    return;
  }

  memory_holder_->Allocate();
  ++pages_allocated_;
}

void MemorySimulator::Read() {
  if (base::TimeTicks::Now() >= read_deadline_) {
    read_timer_.Stop();
    return;
  }

  if (pages_allocated_ == 0) {
    return;
  }

  memory_holder_->Read();
  ++pages_read_;
}

void MemorySimulator::Write() {
  if (base::TimeTicks::Now() >= write_deadline_) {
    write_timer_.Stop();
    return;
  }

  if (pages_allocated_ == 0) {
    return;
  }

  memory_holder_->Write();
  ++pages_written_;
}

}  // namespace memory_simulator
