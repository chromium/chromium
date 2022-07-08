// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_
#define IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_

#include <cstdint>

#include "ipcz/buffer_id.h"
#include "ipcz/buffer_pool.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/ipcz.h"
#include "ipcz/sublink_id.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;

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

  // Constructs a new NodeLinkMemory over a newly allocated DriverMemory object.
  // The new DriverMemory is returned in `primary_buffer_memory`, while the
  // returned NodeLinkMemory internally retains a mapping of that memory.
  static Allocation Allocate(Ref<Node> node);

  // Constructs a new NodeLinkMemory with BufferId 0 (the primary buffer) mapped
  // from `primary_buffer_memory`. The buffer must have been created and
  // initialized by a prior call to Allocate() above.
  static Ref<NodeLinkMemory> Adopt(Ref<Node> node,
                                   DriverMemory primary_buffer_memory);

  // Exposes the underlying BufferPool which owns all shared buffers for this
  // NodeLinkMemory and which facilitates dynamic allocation of the fragments
  // within.
  BufferPool& buffer_pool() { return buffer_pool_; }

  // Returns a new BufferId which should still be unused by any buffer in this
  // NodeLinkMemory's BufferPool, or that of its peer NodeLinkMemory. When
  // allocating new a buffer to add to the BufferPool, its BufferId should be
  // procured by calling this method.
  BufferId AllocateNewBufferId();

  // Returns the first of `count` newly allocated, contiguous sublink IDs for
  // use on the corresponding NodeLink.
  SublinkId AllocateSublinkIds(size_t count);

 private:
  struct PrimaryBuffer;

  NodeLinkMemory(Ref<Node> node, DriverMemoryMapping primary_buffer);
  ~NodeLinkMemory() override;

  const Ref<Node> node_;

  // The underlying BufferPool. Note that this object is itself thread-safe, so
  // access to it is not synchronized by NodeLinkMemory.
  BufferPool buffer_pool_;

  // Mapping for this link's fixed primary buffer.
  const absl::Span<uint8_t> primary_buffer_memory_;
  PrimaryBuffer& primary_buffer_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_NODE_LINK_MEMORY_H_
