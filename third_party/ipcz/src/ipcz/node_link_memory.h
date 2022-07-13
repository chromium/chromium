// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_
#define IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "ipcz/buffer_id.h"
#include "ipcz/buffer_pool.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/fragment_descriptor.h"
#include "ipcz/fragment_ref.h"
#include "ipcz/ipcz.h"
#include "ipcz/router_link_state.h"
#include "ipcz/sublink_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;
class NodeLink;

// NodeLinkMemory owns and manages all shared memory resource allocation on a
// single NodeLink. Each end of a NodeLink has its own NodeLinkMemory instance
// cooperatively managing the same dynamic pool of memory, shared exclusively
// between the two endpoint nodes.
class NodeLinkMemory : public RefCounted {
 public:
  // The maximum number of initial portals supported on ConnectNode() API calls.
  // The first kMaxInitialPortals SublinkIds on a NodeLinkMemory will always be
  // reserved for use by initial portals.
  static constexpr size_t kMaxInitialPortals = 12;

  NodeLinkMemory(NodeLinkMemory&&);

  // Returned by Allocate().
  struct Allocation {
    // The NodeLinkMemory created by a succesful call to Allocate(), or null if
    // memory could not be allocated. This memory is initialized with a
    // primary buffer (BufferId 0) whose contents have also been appropriately
    // initialized. This object is ready for immediate use by a new NodeLink on
    // the `node` passed to Allocate().
    Ref<NodeLinkMemory> node_link_memory;

    // A handle to the region underlying the new NodeLinkMemory's primary
    // buffer. This should be shared with the corresponding NodeLink's remote
    // node, where it can be passed to Adopt() to establish a new NodeLinkMemory
    // there.
    DriverMemory primary_buffer_memory;
  };

  // Sets a reference to the NodeLink using this NodeLinkMemory. This is called
  // by the NodeLink itself before any other methods can be called on the
  // NodeLinkMemory, and it's only reset to null once the NodeLink is
  // deactivated. This link may be used to share information with the remote
  // node, where another NodeLinkMemory is cooperatively managing the same
  // memory pool as this one.
  void SetNodeLink(Ref<NodeLink> link);

  // Constructs a new NodeLinkMemory over a newly allocated DriverMemory object.
  // The new DriverMemory is returned in `primary_buffer_memory`, while the
  // returned NodeLinkMemory internally retains a mapping of that memory.
  static Allocation Allocate(Ref<Node> node);

  // Constructs a new NodeLinkMemory with BufferId 0 (the primary buffer) mapped
  // from `primary_buffer_memory`. The buffer must have been created and
  // initialized by a prior call to Allocate() above.
  static Ref<NodeLinkMemory> Adopt(Ref<Node> node,
                                   DriverMemory primary_buffer_memory);

  // Returns a new BufferId which should still be unused by any buffer in this
  // NodeLinkMemory's BufferPool, or that of its peer NodeLinkMemory. When
  // allocating new a buffer to add to the BufferPool, its BufferId should be
  // procured by calling this method.
  BufferId AllocateNewBufferId();

  // Returns the first of `count` newly allocated, contiguous sublink IDs for
  // use on the corresponding NodeLink.
  SublinkId AllocateSublinkIds(size_t count);

  // Returns a ref to the RouterLinkState for the `i`th initial portal on the
  // NodeLink, established by the Connect() call which created this link. Unlike
  // other RouterLinkStates which are allocated dynamically, these have a fixed
  // location within the NodeLinkMemory's primary buffer. The returned
  // FragmentRef is unmanaged and will never free its underlying fragment.
  FragmentRef<RouterLinkState> GetInitialRouterLinkState(size_t i);

  // Resolves `descriptor` to a concrete Fragment. If the descriptor is null or
  // describes a region of memory which exceeds the bounds of the identified
  // buffer, this returns a null Fragment. If the descriptor's BufferId is not
  // yet registered with this NodeLinkMemory, this returns a pending Fragment
  // with the same BufferId and dimensions as `descriptor`.
  Fragment GetFragment(const FragmentDescriptor& descriptor);

  // Adds a new buffer to the underlying BufferPool to use as additional
  // allocation capacity for blocks of size `block_size`. Note that the
  // contents of the mapped region must already be initialized as a
  // BlockAllocator.
  bool AddBlockBuffer(BufferId id,
                      size_t block_size,
                      DriverMemoryMapping mapping);

  // Allocates a Fragment of `size` bytes from the underlying BufferPool. May
  // return a null Fragment if there was no readily available capacity.
  Fragment AllocateFragment(size_t size);

  // Frees a Fragment previously allocated through this NodeLinkMemory. Returns
  // true on success. Returns false if `fragment` does not represent an
  // allocated fragment within this NodeLinkMemory.
  bool FreeFragment(const Fragment& fragment);

  // Runs `callback` as soon as the identified buffer is added to the underlying
  // BufferPool. If the buffer is already present here, `callback` is run
  // immediately.
  void WaitForBufferAsync(BufferId id,
                          BufferPool::WaitForBufferCallback callback);

 private:
  struct PrimaryBuffer;

  NodeLinkMemory(Ref<Node> node, DriverMemoryMapping primary_buffer);
  ~NodeLinkMemory() override;

  // Indicates whether the NodeLinkMemory should be allowed to expand its
  // allocation capacity further for blocks of size `block_size`.
  bool CanExpandBlockCapacity(size_t block_size);

  // Attempts to expand the total block allocation capacity for blocks of
  // `block_size` bytes. `callback` may be called synchronously or
  // asynchronously with a result indicating whether the expansion succeeded.
  using RequestBlockCapacityCallback = std::function<void(bool)>;
  void RequestBlockCapacity(size_t block_size,
                            RequestBlockCapacityCallback callback);
  void OnCapacityRequestComplete(size_t block_size, bool success);

  const Ref<Node> node_;

  // The underlying BufferPool. Note that this object is itself thread-safe, so
  // access to it is not synchronized by NodeLinkMemory.
  BufferPool buffer_pool_;

  // Mapping for this link's fixed primary buffer.
  const absl::Span<uint8_t> primary_buffer_memory_;
  PrimaryBuffer& primary_buffer_;

  absl::Mutex mutex_;

  // The NodeLink which is using this NodeLinkMemory. Used to communicate with
  // the NodeLinkMemory on the other side of the link.
  Ref<NodeLink> node_link_ ABSL_GUARDED_BY(mutex_);

  // Callbacks to invoke when a pending capacity request is fulfilled for a
  // specific block size. Also used to prevent stacking of capacity requests for
  // the same block size.
  using CapacityCallbackList = std::vector<RequestBlockCapacityCallback>;
  absl::flat_hash_map<uint32_t, CapacityCallbackList> capacity_callbacks_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_
