// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/buffer_pool.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "ipcz/block_allocator_pool.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/numeric/bits.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IpczBlockAllocationSize)
enum class BlockAllocationSize {
  kOther = 0,
  k64B = 1,
  k128B = 2,
  k256B = 3,
  k512B = 4,
  k1KB = 5,
  k2KB = 6,
  k4KB = 7,
  kMaxValue = k4KB,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:IpczBlockAllocationSize)

BlockAllocationSize BlockSizeToBucket(size_t block_size) {
  switch (block_size) {
    case 64:
      return BlockAllocationSize::k64B;
    case 128:
      return BlockAllocationSize::k128B;
    case 256:
      return BlockAllocationSize::k256B;
    case 512:
      return BlockAllocationSize::k512B;
    case 1024:
      return BlockAllocationSize::k1KB;
    case 2048:
      return BlockAllocationSize::k2KB;
    case 4096:
      return BlockAllocationSize::k4KB;
    default:
      return BlockAllocationSize::kOther;
  }
}

void RecordAllocateBlockResult(size_t block_size, bool success) {
  if (!base::ShouldRecordSubsampledMetric(0.001)) {
    return;
  }
  UMA_HISTOGRAM_BOOLEAN("Mojo.Ipcz.BufferPoolAllocateBlockResult", success);
  const BlockAllocationSize bucket = BlockSizeToBucket(block_size);
  if (success) {
    UMA_HISTOGRAM_ENUMERATION("Mojo.Ipcz.BufferPoolAllocateBlockSuccessSize",
                              bucket);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Mojo.Ipcz.BufferPoolAllocateBlockFailureSize",
                              bucket);
  }
}

}  // namespace

BufferPool::BufferPool() = default;

BufferPool::~BufferPool() = default;

Fragment BufferPool::GetFragment(const FragmentDescriptor& descriptor) {
  if (descriptor.is_null()) {
    return {};
  }

  absl::MutexLock lock(&mutex_);
  auto it = mappings_.find(descriptor.buffer_id());
  if (it == mappings_.end()) {
    return Fragment::PendingFromDescriptor(descriptor);
  }

  auto& [id, mapping] = *it;
  return Fragment::MappedFromDescriptor(descriptor, mapping);
}

bool BufferPool::AddBlockBuffer(
    BufferId id,
    DriverMemoryMapping mapping,
    absl::Span<const BlockAllocator> block_allocators) {
  ABSL_ASSERT(mapping.is_valid());

  // Basic consistency checks before we change any pool state: ensure that each
  // given allocator actually lives within the memory mapped by `mapping`, and
  // that each has a unique power-of-2 block size.
  size_t block_sizes_present = 0;
  for (const auto& allocator : block_allocators) {
    if (&allocator.region().front() < &mapping.bytes().front() ||
        &allocator.region().back() > &mapping.bytes().back()) {
      // Not a valid allocator region for this mapping.
      return false;
    }

    const size_t block_size = allocator.block_size();
    ABSL_ASSERT(block_size >= 8 && block_size <= (1 << 30));
    if (!absl::has_single_bit(block_size)) {
      // Not a power of two.
      return false;
    }

    if (block_sizes_present & block_size) {
      // Duplicate allocator block size for this buffer.
      return false;
    }

    block_sizes_present |= block_size;
  }

  std::vector<WaitForBufferCallback> callbacks;
  {
    absl::MutexLock lock(&mutex_);
    auto [it, inserted] = mappings_.insert({id, std::move(mapping)});
    if (!inserted) {
      return false;
    }

    auto callbacks_it = buffer_callbacks_.find(id);
    if (callbacks_it != buffer_callbacks_.end()) {
      callbacks = std::move(callbacks_it->second);
      buffer_callbacks_.erase(callbacks_it);
    }

    auto& inserted_mapping = it->second;
    for (const auto& allocator : block_allocators) {
      const size_t block_size = allocator.block_size();
      auto [pool_it, pool_inserted] =
          block_allocator_pools_.insert({block_size, nullptr});
      auto& pool = pool_it->second;
      if (pool_inserted) {
        pool = std::make_unique<BlockAllocatorPool>();
      }
      pool->Add(id, inserted_mapping.bytes(), allocator);
    }
  }

  for (auto& callback : callbacks) {
    callback();
  }

  return true;
}

size_t BufferPool::GetTotalBlockCapacity(size_t block_size) {
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

Fragment BufferPool::AllocateBlock(size_t block_size) {
  ABSL_ASSERT(absl::has_single_bit(block_size));

  BlockAllocatorPool* pool;
  {
    absl::MutexLock lock(&mutex_);
    auto it = block_allocator_pools_.lower_bound(block_size);
    if (it == block_allocator_pools_.end()) {
      RecordAllocateBlockResult(block_size, false);
      return {};
    }

    // NOTE: BlockAllocatorPools live as long as this BufferPool once added, and
    // they are thread-safe objects; so retaining this pointer through the
    // extent of AllocateBlock() is safe.
    pool = it->second.get();
  }

  Fragment fragment = pool->Allocate();
  RecordAllocateBlockResult(block_size, !fragment.is_null());
  return fragment;
}

Fragment BufferPool::AllocateBlockBestEffort(size_t preferred_block_size) {
  ABSL_ASSERT(absl::has_single_bit(preferred_block_size));

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

    pool_iter = block_allocator_pools_.lower_bound(preferred_block_size);
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

bool BufferPool::FreeBlock(const Fragment& fragment) {
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

void BufferPool::WaitForBufferAsync(BufferId id,
                                    WaitForBufferCallback callback) {
  {
    absl::MutexLock lock(&mutex_);
    auto it = mappings_.find(id);
    if (it == mappings_.end()) {
      buffer_callbacks_[id].push_back(std::move(callback));
      return;
    }
  }

  callback();
}

void BufferPool::DropPendingBufferCallbacks() {
  // Dropping callbacks may have side-effects, so do it outside of a lock.
  BufferCallbackMap buffer_callbacks;
  {
    absl::MutexLock lock(&mutex_);
    buffer_callbacks.swap(buffer_callbacks_);
  }
}

}  // namespace ipcz
