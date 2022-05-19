// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link_memory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "ipcz/buffer_id.h"
#include "ipcz/driver_memory.h"
#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

constexpr BufferId kPrimaryBufferId{0};

// Fixed allocation size for each NodeLink's primary shared buffer.
constexpr size_t kPrimaryBufferSize = 65536;

}  // namespace

// This structure always sits at offset 0 in the primary buffer and has a fixed
// layout according to the NodeLink's agreed upon protocol version. This is the
// layout for version 0 (currently the only version.)
struct IPCZ_ALIGN(8) NodeLinkMemory::PrimaryBuffer {
  // Atomic generator for new unique BufferIds to use across the associated
  // NodeLink. This allows each side of a NodeLink to generate new BufferIds
  // spontaneously without synchronization or risk of collisions.
  std::atomic<uint64_t> next_buffer_id;

  // Atomic generator for new unique SublinkIds to use across the associated
  // NodeLink. This allows each side of a NodeLink to generate new SublinkIds
  // spontaneously without synchronization or risk of collisions.
  std::atomic<uint64_t> next_sublink_id;
};

NodeLinkMemory::NodeLinkMemory(Ref<Node> node,
                               DriverMemoryMapping primary_buffer_memory)
    : node_(std::move(node)),
      primary_buffer_memory_(primary_buffer_memory.bytes()),
      primary_buffer_(
          *reinterpret_cast<PrimaryBuffer*>(primary_buffer_memory_.data())) {
  // Consistency check here, because PrimaryBuffer is private to NodeLinkMemory.
  static_assert(sizeof(PrimaryBuffer) <= kPrimaryBufferSize,
                "PrimaryBuffer structure is too large.");

  buffer_pool_.AddBuffer(BufferId{kPrimaryBufferId},
                         std::move(primary_buffer_memory));
}

NodeLinkMemory::~NodeLinkMemory() = default;

// static
NodeLinkMemory::Allocation NodeLinkMemory::Allocate(Ref<Node> node) {
  DriverMemory primary_buffer_memory(node, sizeof(PrimaryBuffer));
  if (!primary_buffer_memory.is_valid()) {
    return {.node_link_memory = nullptr, .primary_buffer_memory = {}};
  }

  auto memory = AdoptRef(
      new NodeLinkMemory(std::move(node), primary_buffer_memory.Map()));

  PrimaryBuffer& primary_buffer = memory->primary_buffer_;

  // The first allocable BufferId is 1, because the primary buffer uses 0.
  primary_buffer.next_buffer_id.store(1, std::memory_order_relaxed);

  // The first allocable SublinkId is kMaxInitialPortals. This way it doesn't
  // matter whether the two ends of a NodeLink initiate their connection with a
  // different initial portal count: neither can request more than
  // kMaxInitialPortals, so neither will be assuming initial ownership of any
  // SublinkIds at or above this value.
  primary_buffer.next_sublink_id.store(kMaxInitialPortals,
                                       std::memory_order_release);

  return {
      .node_link_memory = std::move(memory),
      .primary_buffer_memory = std::move(primary_buffer_memory),
  };
}

// static
Ref<NodeLinkMemory> NodeLinkMemory::Adopt(Ref<Node> node,
                                          DriverMemory primary_buffer_memory) {
  return AdoptRef(
      new NodeLinkMemory(std::move(node), primary_buffer_memory.Map()));
}

BufferId NodeLinkMemory::AllocateNewBufferId() {
  return BufferId{
      primary_buffer_.next_buffer_id.fetch_add(1, std::memory_order_relaxed)};
}

SublinkId NodeLinkMemory::AllocateSublinkIds(size_t count) {
  return SublinkId{primary_buffer_.next_sublink_id.fetch_add(
      count, std::memory_order_relaxed)};
}

}  // namespace ipcz
