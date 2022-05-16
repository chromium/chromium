// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/buffer_pool.h"

#include <vector>

#include "ipcz/block_allocator.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/node.h"
#include "reference_drivers/single_process_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

class BufferPoolTest : public testing::Test {
 protected:
  DriverMemoryMapping AllocateDriverMemory(size_t size) {
    return DriverMemory(node_, size).Map();
  }

 private:
  const Ref<Node> node_{
      MakeRefCounted<Node>(Node::Type::kBroker,
                           reference_drivers::kSingleProcessReferenceDriver,
                           IPCZ_INVALID_DRIVER_HANDLE)};
};

TEST_F(BufferPoolTest, AddBuffer) {
  constexpr size_t kSize = 4096;
  DriverMemoryMapping mapping = AllocateDriverMemory(kSize);
  absl::Span<uint8_t> bytes = mapping.bytes();

  BufferPool pool;
  EXPECT_TRUE(pool.AddBuffer(BufferId{0}, std::move(mapping)));

  auto memory = pool.GetBufferMemory(BufferId{0});
  EXPECT_EQ(bytes.data(), memory.data());
  EXPECT_EQ(bytes.size(), memory.size());

  // No duplicates.
  DriverMemoryMapping another_mapping = AllocateDriverMemory(kSize);
  EXPECT_FALSE(pool.AddBuffer(BufferId{0}, std::move(another_mapping)));

  // BufferId 0 is still the original buffer.
  memory = pool.GetBufferMemory(BufferId{0});
  EXPECT_EQ(bytes.data(), memory.data());
  EXPECT_EQ(bytes.size(), memory.size());

  DriverMemoryMapping yet_another_mapping = AllocateDriverMemory(kSize);
  absl::Span<uint8_t> other_bytes = yet_another_mapping.bytes();
  EXPECT_TRUE(pool.AddBuffer(BufferId{1}, std::move(yet_another_mapping)));

  // BufferId 0 is still the original buffer.
  memory = pool.GetBufferMemory(BufferId{0});
  EXPECT_EQ(bytes.data(), memory.data());
  EXPECT_EQ(bytes.size(), memory.size());

  // BufferId 1 is available now too.
  memory = pool.GetBufferMemory(BufferId{1});
  EXPECT_EQ(other_bytes.data(), memory.data());
  EXPECT_EQ(other_bytes.size(), memory.size());
}

TEST_F(BufferPoolTest, GetFragment) {
  constexpr size_t kSize1 = 4096;
  constexpr size_t kSize2 = 2048;
  DriverMemoryMapping mapping1 = AllocateDriverMemory(kSize1);
  DriverMemoryMapping mapping2 = AllocateDriverMemory(kSize2);
  absl::Span<uint8_t> bytes1 = mapping1.bytes();
  absl::Span<uint8_t> bytes2 = mapping2.bytes();

  BufferPool pool;
  EXPECT_TRUE(pool.AddBuffer(BufferId{1}, std::move(mapping1)));
  EXPECT_TRUE(pool.AddBuffer(BufferId{2}, std::move(mapping2)));

  // We can resolve fragments covering entire buffers.
  Fragment fragment =
      pool.GetFragment(FragmentDescriptor{BufferId{1}, 0, kSize1});
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes1.data(), fragment.bytes().data());
  EXPECT_EQ(kSize1, fragment.bytes().size());

  fragment = pool.GetFragment(FragmentDescriptor{BufferId{2}, 0, kSize2});
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(bytes2.data(), fragment.bytes().data());
  EXPECT_EQ(kSize2, fragment.bytes().size());

  // We can resolve fragments covering a subspan of a buffer.
  constexpr size_t kPartialFragmentOffset = 4;
  constexpr size_t kPartialFragmentSize = kSize2 / 2;
  fragment = pool.GetFragment(FragmentDescriptor{
      BufferId{2}, kPartialFragmentOffset, kPartialFragmentSize});
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
  EXPECT_EQ(descriptor.buffer_id(), fragment.buffer_id());
  EXPECT_EQ(descriptor.offset(), fragment.offset());
  EXPECT_EQ(descriptor.size(), fragment.size());

  // Null descriptors resolve to null fragments.
  fragment = pool.GetFragment({});
  EXPECT_TRUE(fragment.is_null());

  // Out-of-bounds descriptors resolve to null fragments too.
  fragment = pool.GetFragment(FragmentDescriptor{BufferId{1}, 0, kSize1 + 1});
  EXPECT_TRUE(fragment.is_null());
}

TEST_F(BufferPoolTest, BasicBlockAllocation) {
  BufferPool pool;
  pool.AddBuffer(BufferId{0}, AllocateDriverMemory(4096));
  pool.AddBuffer(BufferId{1}, AllocateDriverMemory(4096));

  constexpr size_t kBlockSize = 64;
  BlockAllocator allocator1(pool.GetBufferMemory(BufferId{0}), kBlockSize);
  allocator1.InitializeRegion();

  BlockAllocator allocator2(pool.GetBufferMemory(BufferId{1}), kBlockSize);
  allocator2.InitializeRegion();

  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{0}, allocator1));

  // No duplicates.
  EXPECT_FALSE(pool.RegisterBlockAllocator(BufferId{0}, allocator2));

  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{1}, allocator2));

  EXPECT_EQ(kBlockSize * (allocator1.capacity() + allocator2.capacity()),
            pool.GetTotalBlockAllocatorCapacity(kBlockSize));

  // We can't free something that isn't a valid allocation.
  EXPECT_FALSE(pool.FreeFragment(Fragment{{}, nullptr}));
  EXPECT_FALSE(pool.FreeFragment(Fragment{{BufferId{1000}, 0, 1}, nullptr}));
  EXPECT_FALSE(pool.FreeFragment(
      Fragment{{BufferId{0}, 0, 1}, pool.GetBufferMemory(BufferId{0}).data()}));

  // Allocate all available capacity.
  std::vector<Fragment> fragments;
  for (;;) {
    Fragment fragment = pool.AllocateFragment(kBlockSize);
    if (fragment.is_null()) {
      break;
    }
    fragments.push_back(fragment);
  }

  EXPECT_EQ(allocator1.capacity() + allocator2.capacity(), fragments.size());
  for (const Fragment& fragment : fragments) {
    EXPECT_TRUE(pool.FreeFragment(fragment));
  }
}

TEST_F(BufferPoolTest, BlockAllocationSizing) {
  BufferPool pool;
  EXPECT_TRUE(pool.AddBuffer(BufferId{1}, AllocateDriverMemory(4096)));
  EXPECT_TRUE(pool.AddBuffer(BufferId{2}, AllocateDriverMemory(4096)));

  constexpr size_t kBuffer1BlockSize = 64;
  BlockAllocator allocator1(pool.GetBufferMemory(BufferId{1}),
                            kBuffer1BlockSize);
  allocator1.InitializeRegion();

  constexpr size_t kBuffer2BlockSize = 128;
  BlockAllocator allocator2(pool.GetBufferMemory(BufferId{2}),
                            kBuffer2BlockSize);
  allocator2.InitializeRegion();

  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{1}, allocator1));
  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{2}, allocator2));

  // Allocations not larger than 64 bytes should be drawn from buffer 1.

  Fragment fragment = pool.AllocateFragment(1);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId{1}, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  fragment = pool.AllocateFragment(kBuffer1BlockSize / 2);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId{1}, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  fragment = pool.AllocateFragment(kBuffer1BlockSize);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId{1}, fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, fragment.size());

  // Larger allocations which are still no larger than 128 bytes should be drawn
  // from buffer 2.

  fragment = pool.AllocateFragment(kBuffer1BlockSize + 1);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId{2}, fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, fragment.size());

  fragment = pool.AllocateFragment(kBuffer2BlockSize);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId{2}, fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, fragment.size());

  // Anything larger than kBuffer2BlockSize should fail to allocate.

  fragment = pool.AllocateFragment(kBuffer2BlockSize + 1);
  EXPECT_TRUE(fragment.is_null());
}

TEST_F(BufferPoolTest, PartialBlockAllocation) {
  BufferPool pool;
  EXPECT_TRUE(pool.AddBuffer(BufferId{1}, AllocateDriverMemory(4096)));
  EXPECT_TRUE(pool.AddBuffer(BufferId{2}, AllocateDriverMemory(4096)));

  constexpr size_t kBuffer1BlockSize = 64;
  BlockAllocator allocator1(pool.GetBufferMemory(BufferId{1}),
                            kBuffer1BlockSize);
  allocator1.InitializeRegion();

  constexpr size_t kBuffer2BlockSize = 128;
  BlockAllocator allocator2(pool.GetBufferMemory(BufferId{2}),
                            kBuffer2BlockSize);
  allocator2.InitializeRegion();

  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{1}, allocator1));
  EXPECT_TRUE(pool.RegisterBlockAllocator(BufferId{2}, allocator2));

  // Oversized partial allocations can succceed.

  Fragment partial_fragment =
      pool.AllocatePartialFragment(kBuffer2BlockSize + 1);
  EXPECT_TRUE(partial_fragment.is_addressable());
  EXPECT_EQ(BufferId{2}, partial_fragment.buffer_id());
  EXPECT_EQ(kBuffer2BlockSize, partial_fragment.size());

  // If we exhaust a sufficient block size, we should fall back onto smaller
  // block sizes.

  // First allocate all available capacity for kBuffer2BlockSize.
  std::vector<Fragment> fragments;
  for (;;) {
    Fragment fragment = pool.AllocateFragment(kBuffer2BlockSize);
    if (fragment.is_null()) {
      break;
    }
    fragments.push_back(fragment);
  }

  // A partial allocation of kBuffer2BlockSize should still succeed, albeit for
  // a smaller size (kBuffer1BlockSize).

  partial_fragment = pool.AllocatePartialFragment(kBuffer2BlockSize);
  EXPECT_TRUE(partial_fragment.is_addressable());
  EXPECT_EQ(BufferId{1}, partial_fragment.buffer_id());
  EXPECT_EQ(kBuffer1BlockSize, partial_fragment.size());
}

}  // namespace
}  // namespace ipcz
