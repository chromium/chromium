// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/block_allocator.h"

#include <atomic>
#include <cstring>
#include <set>
#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {
namespace {

constexpr size_t kPageSize = 16384;
constexpr size_t kBlockSize = 32;

class BlockAllocatorTest : public testing::Test {
 public:
  BlockAllocatorTest() { allocator_.InitializeRegion(); }

  const BlockAllocator& allocator() const { return allocator_; }

 private:
  uint8_t page_[kPageSize];
  const BlockAllocator allocator_{page_, kBlockSize};
};

TEST_F(BlockAllocatorTest, Basic) {
  // Basic consistency check for a single thread accessing the BlockAllocator
  // sequentially. Blocks are allocated, fully overwritten, and then freed.

  std::set<void*> blocks;
  for (size_t i = 0; i < allocator().capacity(); ++i) {
    void* block = allocator().Allocate();
    ASSERT_TRUE(block);
    memset(block, 0xaa, kBlockSize);
    auto [it, inserted] = blocks.insert(block);
    EXPECT_TRUE(inserted);
  }

  // All capacity has been allocated, so Allocate() should fail.
  EXPECT_FALSE(allocator().Allocate());

  // The entire region (except the first block) should be filled with 0xaa
  // bytes, as all blocks were allocated and filled completely.
  constexpr size_t kNumAllocatedBytes = kPageSize - kBlockSize;
  char expected_data[kNumAllocatedBytes];
  memset(expected_data, 0xaa, kNumAllocatedBytes);
  EXPECT_EQ(0, memcmp(allocator().region().data() + kBlockSize, expected_data,
                      kNumAllocatedBytes));

  for (void* block : blocks) {
    EXPECT_TRUE(allocator().Free(block));
  }
}

TEST_F(BlockAllocatorTest, AllocUseFreeRace) {
  // Spins up a worker thread to allocate new blocks and write to them
  // non-atomically, along with a separate worker thread to free them. This
  // effectively exercises the various constraints on memory access ordering
  // within BlockAllocator and in conjunction with TSan should be sufficient to
  // reveal any problematic raciness.

  static constexpr size_t kNumIterations = 1000;
  std::vector<std::atomic<void*>> allocated_blocks(kNumIterations);
  std::thread alloc_thread([this, &allocated_blocks] {
    for (size_t i = 0; i < kNumIterations; ++i) {
      size_t* data;
      do {
        data = static_cast<size_t*>(allocator().Allocate());
      } while (!data);
      *data = i;
      allocated_blocks[i].store(data, std::memory_order_release);
    }
  });

  std::thread free_thread([this, &allocated_blocks] {
    for (size_t i = 0; i < kNumIterations; ++i) {
      size_t* data;
      do {
        data = static_cast<size_t*>(
            allocated_blocks[i].load(std::memory_order_acquire));
      } while (!data);

      EXPECT_EQ(i, *data);
      EXPECT_TRUE(allocator().Free(data));
    }
  });

  alloc_thread.join();
  free_thread.join();
}

TEST_F(BlockAllocatorTest, StressTest) {
  // This test creates a bunch of worker threads to race Allocate() and Free()
  // operations. Workers mark their allocated blocks and verify consistency of
  // markers when freeing them. Once all workers have finished, the test
  // verifies that all blocks are still allocable and none were lost due to racy
  // accounting errors.

  static constexpr size_t kNumIterationsPerWorker = 1000;
  static constexpr size_t kNumAllocationsPerIteration = 50;
  auto worker = [this](uint32_t id) {
    std::atomic<uint32_t>* allocations[kNumAllocationsPerIteration] = {};
    for (size_t i = 0; i < kNumIterationsPerWorker; ++i) {
      size_t num_allocations = 0;
      for (size_t j = 0; j < kNumAllocationsPerIteration; ++j) {
        if (auto* p =
                static_cast<std::atomic<uint32_t>*>(allocator().Allocate())) {
          allocations[num_allocations++] = p;
          p->store(id, std::memory_order_relaxed);
        }
      }
      for (size_t j = 0; j < num_allocations; ++j) {
        std::atomic<uint32_t>* p = allocations[j];
        EXPECT_EQ(id, p->load(std::memory_order_relaxed));
        EXPECT_TRUE(allocator().Free(p));
      }
    }
  };

  static constexpr uint32_t kNumWorkers = 4;
  std::vector<std::thread> worker_threads;
  for (uint32_t i = 0; i < kNumWorkers; ++i) {
    worker_threads.emplace_back(worker, i);
  }

  for (auto& t : worker_threads) {
    t.join();
  }

  size_t allocable_capacity = 0;
  for (size_t i = 0; i < allocator().capacity(); ++i) {
    void* p = allocator().Allocate();
    if (p) {
      ++allocable_capacity;
    }
  }

  EXPECT_EQ(allocator().capacity(), allocable_capacity);
}

}  // namespace
}  // namespace ipcz
