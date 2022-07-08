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

// The front of the primary buffer is reserved for special current and future
// uses which require synchronous availability throughout a link's lifetime.
constexpr size_t kPrimaryBufferReservedHeaderSize = 256;

struct IPCZ_ALIGN(8) PrimaryBufferHeader {
  // Atomic generator for new unique BufferIds to use across the associated
  // NodeLink. This allows each side of a NodeLink to generate new BufferIds
  // spontaneously without synchronization or risk of collisions.
  std::atomic<uint64_t> next_buffer_id;

  // Atomic generator for new unique SublinkIds to use across the associated
  // NodeLink. This allows each side of a NodeLink to generate new SublinkIds
  // spontaneously without synchronization or risk of collisions.
  std::atomic<uint64_t> next_sublink_id;
};

static_assert(sizeof(PrimaryBufferHeader) < kPrimaryBufferReservedHeaderSize);

constexpr size_t kPrimaryBufferHeaderPaddingSize =
    kPrimaryBufferReservedHeaderSize - sizeof(PrimaryBufferHeader);

}  // namespace

// This structure always sits at offset 0 in the primary buffer and has a fixed
// layout according to the NodeLink's agreed upon protocol version. This is the
// layout for version 0 (currently the only version.)
struct IPCZ_ALIGN(8) NodeLinkMemory::PrimaryBuffer {
  // Header + padding occupies the first 256 bytes.
  PrimaryBufferHeader header;
  uint8_t reserved_header_padding[kPrimaryBufferHeaderPaddingSize];

  // Reserved memory for a series of fixed block allocators. Additional
  // allocators may be adopted by a NodeLinkMemory over its lifetime, but these
  // ones remain fixed within the primary buffer.
  std::array<uint8_t, 4096> mem_for_64_byte_blocks;
  std::array<uint8_t, 12288> mem_for_256_byte_blocks;
  std::array<uint8_t, 15360> mem_for_512_byte_blocks;
  std::array<uint8_t, 11264> mem_for_1024_byte_blocks;
  std::array<uint8_t, 16384> mem_for_2048_byte_blocks;

  BlockAllocator block_allocator_64() {
    return BlockAllocator(absl::MakeSpan(mem_for_64_byte_blocks), 64);
  }

  BlockAllocator block_allocator_256() {
    return BlockAllocator(absl::MakeSpan(mem_for_256_byte_blocks), 256);
  }

  BlockAllocator block_allocator_512() {
    return BlockAllocator(absl::MakeSpan(mem_for_512_byte_blocks), 512);
  }

  BlockAllocator block_allocator_1024() {
    return BlockAllocator(absl::MakeSpan(mem_for_1024_byte_blocks), 1024);
  }

  BlockAllocator block_allocator_2048() {
    return BlockAllocator(absl::MakeSpan(mem_for_2048_byte_blocks), 2048);
  }
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

  buffer_pool_.AddBuffer(kPrimaryBufferId, std::move(primary_buffer_memory));
  buffer_pool_.RegisterBlockAllocator(kPrimaryBufferId,
                                      primary_buffer_.block_allocator_64());
  buffer_pool_.RegisterBlockAllocator(kPrimaryBufferId,
                                      primary_buffer_.block_allocator_256());
  buffer_pool_.RegisterBlockAllocator(kPrimaryBufferId,
                                      primary_buffer_.block_allocator_512());
  buffer_pool_.RegisterBlockAllocator(kPrimaryBufferId,
                                      primary_buffer_.block_allocator_1024());
  buffer_pool_.RegisterBlockAllocator(kPrimaryBufferId,
                                      primary_buffer_.block_allocator_2048());
}

NodeLinkMemory::~NodeLinkMemory() = default;

// static
NodeLinkMemory::Allocation NodeLinkMemory::Allocate(Ref<Node> node) {
  DriverMemory primary_buffer_memory(node->driver(), sizeof(PrimaryBuffer));
  if (!primary_buffer_memory.is_valid()) {
    return {.node_link_memory = nullptr, .primary_buffer_memory = {}};
  }

  auto memory = AdoptRef(
      new NodeLinkMemory(std::move(node), primary_buffer_memory.Map()));

  PrimaryBuffer& primary_buffer = memory->primary_buffer_;

  // The first allocable BufferId is 1, because the primary buffer uses 0.
  primary_buffer.header.next_buffer_id.store(1, std::memory_order_relaxed);

  // The first allocable SublinkId is kMaxInitialPortals. This way it doesn't
  // matter whether the two ends of a NodeLink initiate their connection with a
  // different initial portal count: neither can request more than
  // kMaxInitialPortals, so neither will be assuming initial ownership of any
  // SublinkIds at or above this value.
  primary_buffer.header.next_sublink_id.store(kMaxInitialPortals,
                                              std::memory_order_release);

  primary_buffer.block_allocator_64().InitializeRegion();
  primary_buffer.block_allocator_256().InitializeRegion();
  primary_buffer.block_allocator_512().InitializeRegion();
  primary_buffer.block_allocator_1024().InitializeRegion();
  primary_buffer.block_allocator_2048().InitializeRegion();

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
  return BufferId{primary_buffer_.header.next_buffer_id.fetch_add(
      1, std::memory_order_relaxed)};
}

SublinkId NodeLinkMemory::AllocateSublinkIds(size_t count) {
  return SublinkId{primary_buffer_.header.next_sublink_id.fetch_add(
      count, std::memory_order_relaxed)};
}

}  // namespace ipcz
