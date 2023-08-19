// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/buffer_pool.h"

#include <vector>

#include "ipcz/block_allocator.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/node.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

class BufferPoolTest : public testing::Test {
 protected:
  DriverMemoryMapping AllocateDriverMemory(size_t size) {
    return DriverMemory(node_->driver(), size).Map();
  }

 private:
  const Ref<Node> node_{
      MakeRefCounted<Node>(Node::Type::kBroker,
                           reference_drivers::kSyncReferenceDriver)};
};

TEST_F(BufferPoolTest, AddBlockBuffer) {
  constexpr size_t kBufferSize = 4096;
  constexpr size_t kBlockSize = 64;
  DriverMemoryMapping mapping = AllocateDriverMemory(kBufferSize);
  const absl::Span<uint8_t> bytes = mapping.bytes();
  const BlockAllocator allocators[] = {{bytes, kBlockSize}};
  constexpr BufferId id(0);
  BufferPool pool;
  EXPECT_TRUE(pool.AddBlockBuffer(id, std::move(mapping), allocators));

  Fragment fragment = pool.GetFragment({id, 0, kBufferSize});
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes.data(), fragment.bytes().data());
  EXPECT_EQ(bytes.size(), fragment.bytes().size());
}

TEST_F(BufferPoolTest, AddBlockBufferNoDuplicateBufferId) {
  constexpr size_t kBufferSize = 4096;
  constexpr size_t kBlockSize = 64;
  DriverMemoryMapping mapping = AllocateDriverMemory(kBufferSize);
  const absl::Span<uint8_t> bytes = mapping.bytes();
  const BlockAllocator allocators[] = {{bytes, kBlockSize}};
  constexpr BufferId id(0);
  BufferPool pool;
  EXPECT_TRUE(pool.AddBlockBuffer(id, std::move(mapping), allocators));

  // Adding another buffer with the same ID as above must fail.
  DriverMemoryMapping another_mapping = AllocateDriverMemory(kBufferSize);
  const BlockAllocator another_allocator(another_mapping.bytes(), kBlockSize);
  EXPECT_FALSE(pool.AddBlockBuffer(id, std::move(another_mapping),
                                   {&another_allocator, 1}));

  // Fragment resolution against buffer 0 should still map to the first buffer.
  Fragment fragment = pool.GetFragment({id, 0, kBufferSize});
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes.data(), fragment.bytes().data());
  EXPECT_EQ(bytes.size(), fragment.bytes().size());
}

TEST_F(BufferPoolTest, AddBlockBufferNoDuplicateAllocatorBlockSizes) {
  constexpr size_t kBufferSize = 4096;
  constexpr size_t kBlockSize = 64;
  DriverMemoryMapping mapping = AllocateDriverMemory(kBufferSize);
  const absl::Span<uint8_t> bytes = mapping.bytes();

  // Carve up the buffer into two separate allocators for the same block size,
  // and try to register them both. This is unsupported.
  const BlockAllocator allocators[] = {
      {bytes.subspan(0, kBlockSize * 2), kBlockSize},
      {bytes.subspan(kBlockSize * 2), kBlockSize},
  };

  constexpr BufferId id(0);
  BufferPool pool;
  EXPECT_FALSE(pool.AddBlockBuffer(id, std::move(mapping), allocators));

  // No buffer is registered to resolve this fragment.
  Fragment fragment = pool.GetFragment({id, 0, 8});
  EXPECT_TRUE(fragment.is_pending());
}

TEST_F(BufferPoolTest, AddBlockBufferRequireBlockSizePowerOfTwo) {
  constexpr size_t kBufferSize = 4096;
  constexpr size_t kBadBlockSize = 80;
  DriverMemoryMapping mapping = AllocateDriverMemory(kBufferSize);
  const BlockAllocator bad_allocator(mapping.bytes(), kBadBlockSize);

  constexpr BufferId id(0);
  BufferPool pool;
  EXPECT_FALSE(
      pool.AddBlockBuffer(id, std::move(mapping), {&bad_allocator, 1}));

  // No buffer is registered to resolve this fragment.
  Fragment fragment = pool.GetFragment({id, 0, 8});
  EXPECT_TRUE(fragment.is_pending());
}

TEST_F(BufferPoolTest, GetFragment) {
  constexpr size_t kBufferSize1 = 4096;
  constexpr size_t kBufferSize2 = 2048;
  constexpr size_t kBlockSize = 64;
  DriverMemoryMapping mapping1 = AllocateDriverMemory(kBufferSize1);
  DriverMemoryMapping mapping2 = AllocateDriverMemory(kBufferSize2);
  absl::Span<uint8_t> bytes1 = mapping1.bytes();
  absl::Span<uint8_t> bytes2 = mapping2.bytes();
  BlockAllocator allocators1[] = {{bytes1, kBlockSize}};
  BlockAllocator allocators2[] = {{bytes2, kBlockSize}};

  BufferPool pool;
  constexpr BufferId id1(1);
  constexpr BufferId id2(2);
  EXPECT_TRUE(pool.AddBlockBuffer(id1, std::move(mapping1), allocators1));
  EXPECT_TRUE(pool.AddBlockBuffer(id2, std::move(mapping2), allocators2));

  // We can resolve fragments covering entire buffers.
  Fragment fragment = pool.GetFragment({id1, /*offset=*/0, kBufferSize1});
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes1.data(), fragment.bytes().data());
  EXPECT_EQ(kBufferSize1, fragment.bytes().size());

  fragment = pool.GetFragment({id2, /*offset=*/0, kBufferSize2});
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes2.data(), fragment.bytes().data());
  EXPECT_EQ(kBufferSize2, fragment.bytes().size());

  // We can resolve fragments covering a subspan of a buffer.
  constexpr size_t kPartialFragmentOffset = 4;
  constexpr size_t kPartialFragmentSize = kBufferSize2 / 2;
  fragment =
      pool.GetFragment({id2, kPartialFragmentOffset, kPartialFragmentSize});
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes2.subspan(kPartialFragmentOffset).data(),
            fragment.bytes().data());
  EXPECT_EQ(kPartialFragmentSize, fragment.bytes().size());

  // Unknown BufferIds resolve to pending fragments.
  const FragmentDescriptor descriptor(BufferId{42}, 5, 1234);
  fragment = pool.GetFragment(descriptor);
  EXPECT_FALSE(fragment.is_null());
  EXPECT_FALSE(fragment.is_addressable());
  EXPECT_TRUE(fragment.is_pending());
  EXPECT_EQ(nullptr, fragment.address());
  EXPECT_EQ(descriptor.buffer_id(), fragment.buffer_id());
  EXPECT_EQ(descriptor.offset(), fragment.offset());
  EXPECT_EQ(descriptor.size(), fragment.size());

  // Null descriptors resolve to null fragments.
  fragment = pool.GetFragment({});
  EXPECT_TRUE(fragment.is_null());

  // Out-of-bounds descriptors resolve to null fragments too.
  fragment = pool.GetFragment(FragmentDescriptor{id1, 0, kBufferSize1 + 1});
  EXPECT_TRUE(fragment.is_null());
}

TEST_F(BufferPoolTest, BasicBlockAllocation) {
  constexpr size_t kBufferSize = 4096;
  constexpr size_t kBlockSize = 64;

  auto mapping0 = AllocateDriverMemory(kBufferSize);
  auto mapping1 = AllocateDriverMemory(kBufferSize);
  auto bytes0 = mapping0.bytes();
  auto bytes1 = mapping1.bytes();

  BlockAllocator allocator0(bytes0, kBlockSize);
  allocator0.InitializeRegion();

  BlockAllocator allocator1(bytes1, kBlockSize);
  allocator1.InitializeRegion();

  BufferPool pool;
  constexpr BufferId id0(0);
  constexpr BufferId id1(1);
  EXPECT_TRUE(pool.AddBlockBuffer(id0, std::move(mapping0), {&allocator0, 1}));
  EXPECT_TRUE(pool.AddBlockBuffer(id1, std::move(mapping1), {&allocator1, 1}));

  EXPECT_EQ(kBlockSize * (allocator0.capacity() + allocator1.capacity()),
            pool.GetTotalBlockCapacity(kBlockSize));

  // We can't free something that isn't a valid allocation.
  EXPECT_FALSE(pool.FreeBlock(Fragment::FromDescriptorUnsafe({}, nullptr)));
  EXPECT_FALSE(pool.FreeBlock(
      Fragment::FromDescriptorUnsafe({BufferId{1000}, 0, 1}, nullptr)));
  EXPECT_FALSE(pool.FreeBlock(
      Fragment::FromDescriptorUnsafe({BufferId{0}, 0, 1}, bytes0.data())));

  // Allocate all available capacity.
  std::vector<Fragment> fragments;
  for (;;) {
    Fragment fragment = pool.AllocateBlock(kBlockSize);
    if (fragment.is_null()) {
      break;
    }
    fragments.push_back(fragment);
  }

  EXPECT_EQ(allocator0.capacity() + allocator1.capacity(), fragments.size());
  for (const Fragment& fragment : fragments) {
    EXPECT_TRUE(pool.FreeBlock(fragment));
  }
}

TEST_F(BufferPoolTest, BlockAllocationSizing) {
  constexpr size_t kBufferSize = 4096;
  DriverMemoryMapping mapping1 = AllocateDriverMemory(kBufferSize);
  DriverMemoryMapping mapping2 = AllocateDriverMemory(kBufferSize);

  constexpr size_t kBuffer1BlockSize = 64;
  BlockAllocator allocator1(mapping1.bytes(), kBuffer1BlockSize);
  allocator1.InitializeRegion();

  constexpr size_t kBuffer2BlockSize = kBuffer1BlockSize * 4;
  BlockAllocator allocator2(mapping2.bytes(), kBuffer2BlockSize);
  allocator2.InitializeRegion();

  BufferPool pool;
  constexpr BufferId id1(1);
  constexpr BufferId id2(2);
  EXPECT_TRUE(pool.AddBlockBuffer(id1, std::move(mapping1), {&allocator1, 1}));
  EXPECT_TRUE(pool.AddBlockBuffer(id2, std::move(mapping2), {&allocator2, 1}));

  // Allocations not larger than 64 bytes should be drawn from buffer 1.

  Fragment fragment = pool.AllocateBlock(1);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(id1, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  fragment = pool.AllocateBlock(kBuffer1BlockSize / 2);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(id1, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  fragment = pool.AllocateBlock(kBuffer1BlockSize);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(id1, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  // Larger allocations which are still no larger than kBuffer2BlockSize bytes
  // should be drawn from buffer 2.

  fragment = pool.AllocateBlock(kBuffer1BlockSize * 2);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(id2, fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, fragment.size());

  fragment = pool.AllocateBlock(kBuffer2BlockSize);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(id2, fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, fragment.size());

  // Anything larger than kBuffer2BlockSize should fail to allocate.

  fragment = pool.AllocateBlock(kBuffer2BlockSize * 2);
  EXPECT_TRUE(fragment.is_null());
}

TEST_F(BufferPoolTest, BestEffortBlockAllocation) {
  constexpr size_t kBufferSize = 4096;
  auto mapping1 = AllocateDriverMemory(kBufferSize);
  auto mapping2 = AllocateDriverMemory(kBufferSize);

  constexpr size_t kBuffer1BlockSize = 64;
  BlockAllocator allocator1(mapping1.bytes(), kBuffer1BlockSize);
  allocator1.InitializeRegion();

  constexpr size_t kBuffer2BlockSize = 128;
  BlockAllocator allocator2(mapping2.bytes(), kBuffer2BlockSize);
  allocator2.InitializeRegion();

  BufferPool pool;
  constexpr BufferId id1(1);
  constexpr BufferId id2(2);
  EXPECT_TRUE(pool.AddBlockBuffer(id1, std::move(mapping1), {&allocator1, 1}));
  EXPECT_TRUE(pool.AddBlockBuffer(id2, std::move(mapping2), {&allocator2, 1}));

  // Oversized best-effort allocations can succceed.

  Fragment partial_fragment =
      pool.AllocateBlockBestEffort(kBuffer2BlockSize * 2);
  EXPECT_TRUE(partial_fragment.is_addressable());
  EXPECT_EQ(id2, partial_fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, partial_fragment.size());

  // If we exhaust a sufficient block size, we should fall back onto smaller
  // block sizes. First allocate all available capacity for kBuffer2BlockSize,
  // and then a partial allocation of kBuffer2BlockSize should succeed with a
  // a smaller size (kBuffer1BlockSize).
  while (!pool.AllocateBlock(kBuffer2BlockSize).is_null()) {
  }

  partial_fragment = pool.AllocateBlockBestEffort(kBuffer2BlockSize);
  EXPECT_TRUE(partial_fragment.is_addressable());
  EXPECT_EQ(id1, partial_fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, partial_fragment.size());
}

}  // namespace
}  // namespace ipcz
