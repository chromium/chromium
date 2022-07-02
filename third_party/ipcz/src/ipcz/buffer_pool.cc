// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/buffer_pool.h"

#include <algorithm>

#include "ipcz/block_allocator_pool.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/numeric/bits.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

BufferPool::BufferPool() = default;

BufferPool::~BufferPool() = default;

bool BufferPool::AddBuffer(BufferId id, DriverMemoryMapping mapping) {
  ABSL_ASSERT(mapping.is_valid());

  absl::MutexLock lock(&mutex_);
  auto [it, inserted] = mappings_.insert({id, std::move(mapping)});
  return inserted;
}

absl::Span<uint8_t> BufferPool::GetBufferMemory(BufferId id) {
  absl::MutexLock lock(&mutex_);
  auto it = mappings_.find(id);
  if (it == mappings_.end()) {
    return {};
  }

  return it->second.bytes();
}

Fragment BufferPool::GetFragment(const FragmentDescriptor& descriptor) {
  if (descriptor.is_null()) {
    return {};
  }

  absl::MutexLock lock(&mutex_);
  auto it = mappings_.find(descriptor.buffer_id());
  if (it == mappings_.end()) {
    return Fragment(descriptor, nullptr);
  }

  auto& [id, mapping] = *it;
  if (descriptor.end() > mapping.bytes().size()) {
    return {};
  }

  return Fragment(descriptor, mapping.address_at(descriptor.offset()));
}

bool BufferPool::RegisterBlockAllocator(BufferId buffer_id,
                                        const BlockAllocator& allocator) {
  const size_t block_size = allocator.block_size();

  absl::MutexLock lock(&mutex_);
  auto mapping_it = mappings_.find(buffer_id);
  if (mapping_it == mappings_.end()) {
    return false;
  }

  auto& mapping = mapping_it->second;
  if (&allocator.region().front() < &mapping.bytes().front() ||
      &allocator.region().back() > &mapping.bytes().back()) {
    return false;
  }

  auto [it, inserted] = block_allocator_pools_.insert({block_size, nullptr});
  auto& pool = it->second;
  if (inserted) {
    pool = std::make_unique<BlockAllocatorPool>();
  }

  pool->Add(buffer_id, mapping.bytes(), allocator);
  return true;
}

size_t BufferPool::GetTotalBlockAllocatorCapacity(size_t block_size) {
  BlockAllocatorPool* pool;
  {
    absl::MutexLock lock(&mutex_);
    auto it = block_allocator_pools_.find(block_size);
    if (it == block_allocator_pools_.end()) {
      return 0;
    }

    pool = it->second.get();
  }

  return pool->GetCapacity();
}

Fragment BufferPool::AllocateFragment(size_t num_bytes) {
  const size_t block_size = absl::bit_ceil(num_bytes);
  BlockAllocatorPool* pool;
  {
    absl::MutexLock lock(&mutex_);
    auto it = block_allocator_pools_.lower_bound(block_size);
    if (it == block_allocator_pools_.end()) {
      return {};
    }

    // NOTE: BlockAllocatorPools live as long as this BufferPool once added, and
    // they are thread-safe objects; so retaining this pointer through the
    // extent of AllocateFragment() is safe.
    pool = it->second.get();
  }

  return pool->Allocate();
}

Fragment BufferPool::AllocatePartialFragment(size_t preferred_num_bytes) {
  // Limit the number of attempts we make to scale down the requested size in
  // search of an available fragment. This value was chosen arbitrarily.
  constexpr size_t kMaxAttempts = 3;

  BlockAllocatorPoolMap::iterator pool_iter;
  BlockAllocatorPool* pool;
  {
    absl::MutexLock lock(&mutex_);
    if (block_allocator_pools_.empty()) {
      return {};
    }

    pool_iter = block_allocator_pools_.lower_bound(preferred_num_bytes);
    if (pool_iter == block_allocator_pools_.end()) {
      --pool_iter;
    }

    pool = pool_iter->second.get();
  }

  for (size_t attempts = 0; attempts < kMaxAttempts; ++attempts) {
    const Fragment fragment = pool->Allocate();
    if (!fragment.is_null()) {
      return fragment;
    }

    absl::MutexLock lock(&mutex_);
    if (pool_iter == block_allocator_pools_.begin()) {
      return {};
    }

    --pool_iter;
    pool = pool_iter->second.get();
  }

  return {};
}

bool BufferPool::FreeFragment(const Fragment& fragment) {
  BlockAllocatorPool* pool;
  {
    absl::MutexLock lock(&mutex_);
    auto it = block_allocator_pools_.find(fragment.size());
    if (it == block_allocator_pools_.end()) {
      return false;
    }

    pool = it->second.get();
  }

  return pool->Free(fragment);
}

}  // namespace ipcz
