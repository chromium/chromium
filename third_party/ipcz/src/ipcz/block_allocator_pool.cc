// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/block_allocator_pool.h"

#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/log.h"
#include "util/safe_math.h"

namespace ipcz {

BlockAllocatorPool::BlockAllocatorPool() = default;

BlockAllocatorPool::~BlockAllocatorPool() = default;

size_t BlockAllocatorPool::GetCapacity() {
  absl::MutexLock lock(&mutex_);
  return capacity_;
}

bool BlockAllocatorPool::Add(BufferId buffer_id,
                             absl::Span<uint8_t> buffer_memory,
                             const BlockAllocator& allocator) {
  absl::MutexLock lock(&mutex_);
  Entry* previous_tail = nullptr;
  Entry* new_entry;
  if (!entries_.empty()) {
    previous_tail = &entries_.back();
  }

  capacity_ += allocator.capacity() * allocator.block_size();
  entries_.emplace_back(buffer_id, buffer_memory, allocator);

  new_entry = &entries_.back();
  auto [it, inserted] = entry_map_.insert({buffer_id, new_entry});
  if (!inserted) {
    entries_.pop_back();
    return false;
  }

  if (previous_tail) {
    previous_tail->next = new_entry;
  } else {
    // Balanced by a load-acquire in Allocate().
    active_entry_.store(new_entry, std::memory_order_release);
  }

  return true;
}

Fragment BlockAllocatorPool::Allocate() {
  // For the fast common case, we always start by trying to reuse the most
  // recently used allocator. This load-acquire is balanced by a store-release
  // below (via compare_exchange_weak) and in Add() above.
  Entry* entry = active_entry_.load(std::memory_order_acquire);
  if (!entry) {
    return {};
  }

  Entry* starting_entry = entry;
  do {
    const BlockAllocator& allocator = entry->allocator;
    void* block = allocator.Allocate();
    if (block) {
      // Success! Compute a FragmentDescriptor based on the block address and
      // the mapped region's location.
      const size_t offset =
          (static_cast<uint8_t*>(block) - entry->buffer_memory.data());
      if (entry->buffer_memory.size() - allocator.block_size() < offset) {
        // Allocator did something bad and this span would fall outside of the
        // mapped region's bounds.
        DLOG(ERROR) << "Invalid address from BlockAllocator.";
        return {};
      }

      if (entry != starting_entry) {
        // Attempt to update the active entry to reflect our success. Since this
        // is only meant as a helpful hint for future allocations, we don't
        // really care whether it succeeds.
        active_entry_.compare_exchange_weak(starting_entry, entry,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
      }

      FragmentDescriptor descriptor(
          entry->buffer_id, checked_cast<uint32_t>(offset),
          checked_cast<uint32_t>(allocator.block_size()));
      return Fragment::FromDescriptorUnsafe(descriptor, block);
    }

    // Allocation from the active allocator failed. Try another if available.
    absl::MutexLock lock(&mutex_);
    entry = entry->next;
  } while (entry && entry != starting_entry);

  return {};
}

bool BlockAllocatorPool::Free(const Fragment& fragment) {
  Entry* entry;
  {
    absl::MutexLock lock(&mutex_);
    auto it = entry_map_.find(fragment.buffer_id());
    if (it == entry_map_.end()) {
      DLOG(ERROR) << "Invalid Free() call on BlockAllocatorPool";
      return false;
    }

    entry = it->second;
  }

  return entry->allocator.Free(fragment.address());
}

BlockAllocatorPool::Entry::Entry(BufferId buffer_id,
                                 absl::Span<uint8_t> buffer_memory,
                                 const BlockAllocator& allocator)
    : buffer_id(buffer_id),
      buffer_memory(buffer_memory),
      allocator(allocator) {}

BlockAllocatorPool::Entry::~Entry() = default;

}  // namespace ipcz
