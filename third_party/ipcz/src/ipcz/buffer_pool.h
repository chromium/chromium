// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_BUFFER_POOL_H_
#define IPCZ_SRC_IPCZ_BUFFER_POOL_H_

#include <cstdint>
#include <map>
#include <memory>

#include "ipcz/block_allocator.h"
#include "ipcz/buffer_id.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/fragment.h"
#include "ipcz/fragment_descriptor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

class BlockAllocatorPool;

// BufferPool maintains ownership of an extensible collection of mapped
// DriverMemory buffers, exposing access to them for dynamic memory allocation
// and arbitrary state sharing. Every buffer owned by a BufferPool is identified
// by a unique BufferId, and once a buffer is added to the pool it remains there
// indefinitely.
//
// BufferPool objects are thread-safe.
class BufferPool {
 public:
  BufferPool();
  ~BufferPool();

  // Registers `mapping` under `id` within this pool.
  //
  // Returns true if the mapping was successfully added, or false if the pool
  // already had a buffer registered under the given `id`.
  bool AddBuffer(BufferId id, DriverMemoryMapping mapping);

  // Returns the full span of memory mapped by the identified buffer, or an
  // empty span if no such buffer is registered with this BufferPool.
  //
  // Note that because buffers remain mapped indefinitely by the BufferPool
  // once added, this span is safe to retain as long as the BufferPool itself
  // remains alive.
  absl::Span<uint8_t> GetBufferMemory(BufferId id);

  // Resolves `descriptor` to a concrete Fragment. If the descriptor is null or
  // describes a region of memory which exceeds the bounds of the identified
  // buffer, this returns a null Fragment.
  //
  // If the descriptor's BufferId is not yet registered with this pool, this
  // returns a pending Fragment with the same BufferId and dimensions as
  // `descriptor`.
  //
  // Otherwise this returns a resolved Fragment which references an appropriate
  // span of mapped memory.
  Fragment GetFragment(const FragmentDescriptor& descriptor);

  // Registers a BlockAllocator with this pool to support subsequent
  // AllocateFragment() calls. If successful, the allocator may be used to
  // fulfill fragment allocation requests for any size up to and including
  // `block_size`.
  //
  // `buffer_id` must identify a buffer mapping which has already been
  // registered to this pool via AddBuffer(), and `allocator` must be
  // constructed over a span of memory which falls entirely within that mapping.
  //
  // Returns true on success and false on failure. Failure implies that either
  // `buffer_id` was unknown or `allocator` does not manage memory within the
  // identified buffer.
  bool RegisterBlockAllocator(BufferId buffer_id,
                              const BlockAllocator& allocator);

  // Returns the total size in bytes of capacity available across all registered
  // BlockAllocators for the given `block_size`.
  size_t GetTotalBlockAllocatorCapacity(size_t block_size);

  // Attempts to allocate an unused fragment from the pool with a size of at
  // least `num_bytes`. For most allocations, this prefers to use a
  // BlockAllocator for the smallest available block size which still fits
  // `num_bytes`.
  //
  // If the BufferPool cannot accommodate the allocation request, this returns
  // a null Fragment.
  Fragment AllocateFragment(size_t num_bytes);

  // Similar to AllocateFragment(), but this may allocate less space than
  // requested if that's all that's available. May still return a null Fragment
  // if the BufferPool has trouble finding available memory.
  Fragment AllocatePartialFragment(size_t preferred_num_bytes);

  // Frees a Fragment previously allocated from this pool via AllocateFragment()
  // or AllocatePartialFragment(). Returns true if successful, or false if
  // `fragment` does not identify a fragment allocated from a buffer managed by
  // this pool.
  bool FreeFragment(const Fragment& fragment);

 private:
  absl::Mutex mutex_;
  absl::flat_hash_map<BufferId, DriverMemoryMapping> mappings_
      ABSL_GUARDED_BY(mutex_);

  // Mapping from block size to a pool of BlockAllocators for that size. When
  // a new BlockAllocator is registered with this BufferPool, it's added to an
  // appropriate BlockAllocatorPool within this map.
  using BlockAllocatorPoolMap =
      std::map<size_t, std::unique_ptr<BlockAllocatorPool>>;
  BlockAllocatorPoolMap block_allocator_pools_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_BUFFER_POOL_H_
