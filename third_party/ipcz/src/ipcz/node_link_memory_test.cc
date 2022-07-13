// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link_memory.h"

#include <utility>
#include <vector>

#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_name.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

constexpr NodeName kTestBrokerName(1, 2);
constexpr NodeName kTestNonBrokerName(2, 3);

class NodeLinkMemoryTest : public testing::Test {
 public:
  NodeLinkMemory& memory_a() { return link_a_->memory(); }
  NodeLinkMemory& memory_b() { return link_b_->memory(); }

  void SetUp() override {
    auto transports = DriverTransport::CreatePair(kTestDriver);
    auto alloc = NodeLinkMemory::Allocate(node_a_);
    link_a_ =
        NodeLink::Create(node_a_, LinkSide::kA, kTestBrokerName,
                         kTestNonBrokerName, Node::Type::kNormal, 0,
                         transports.first, std::move(alloc.node_link_memory));
    link_b_ = NodeLink::Create(
        node_b_, LinkSide::kB, kTestNonBrokerName, kTestBrokerName,
        Node::Type::kBroker, 0, transports.second,
        NodeLinkMemory::Adopt(node_b_, std::move(alloc.primary_buffer_memory)));
    node_a_->AddLink(kTestNonBrokerName, link_a_);
    node_b_->AddLink(kTestBrokerName, link_b_);
    link_a_->transport()->Activate();
    link_b_->transport()->Activate();
  }

  void TearDown() override {
    node_b_->Close();
    node_a_->Close();
  }

 private:
  const Ref<Node> node_a_{MakeRefCounted<Node>(Node::Type::kBroker,
                                               kTestDriver,
                                               IPCZ_INVALID_DRIVER_HANDLE)};
  const Ref<Node> node_b_{MakeRefCounted<Node>(Node::Type::kNormal,
                                               kTestDriver,
                                               IPCZ_INVALID_DRIVER_HANDLE)};
  Ref<NodeLink> link_a_;
  Ref<NodeLink> link_b_;
};

TEST_F(NodeLinkMemoryTest, BasicAllocAndFree) {
  Fragment fragment = memory_a().AllocateFragment(64);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_TRUE(fragment.address());
  EXPECT_EQ(fragment.size(), 64u);
  EXPECT_TRUE(memory_a().FreeFragment(fragment));
}

TEST_F(NodeLinkMemoryTest, Zero) {
  // Zero-sized fragments cannot be allocated.
  EXPECT_TRUE(memory_a().AllocateFragment(0).is_null());
}

TEST_F(NodeLinkMemoryTest, MinimumSize) {
  // Very small fragment sizes a minimum of 64 bytes.
  Fragment fragments[] = {
      memory_a().AllocateFragment(1),  memory_a().AllocateFragment(2),
      memory_a().AllocateFragment(3),  memory_a().AllocateFragment(4),
      memory_a().AllocateFragment(17), memory_a().AllocateFragment(63),
  };

  for (const auto& fragment : fragments) {
    EXPECT_TRUE(fragment.is_addressable());
    EXPECT_EQ(64u, fragment.size());
  }
}

TEST_F(NodeLinkMemoryTest, RoundUpSize) {
  // Fragment sizes are rounded up to the nearest power of 2.
  Fragment fragment = memory_a().AllocateFragment(250);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(256u, fragment.size());
}

TEST_F(NodeLinkMemoryTest, SharedPrimaryBuffer) {
  // Test basic allocation from the primary buffer which both NodeLinkMemory
  // instances share from the moment they're constructed. Each NodeLinkMemory
  // should be able to resolve and free fragments allocated by the other.

  Fragment fragment_from_a = memory_a().AllocateFragment(8);
  EXPECT_TRUE(fragment_from_a.is_addressable());
  EXPECT_EQ(BufferId(0), fragment_from_a.buffer_id());
  EXPECT_GE(fragment_from_a.size(), 8u);

  Fragment same_fragment = memory_b().GetFragment(fragment_from_a.descriptor());
  EXPECT_TRUE(same_fragment.is_addressable());
  EXPECT_EQ(fragment_from_a.buffer_id(), same_fragment.buffer_id());
  EXPECT_EQ(fragment_from_a.offset(), same_fragment.offset());
  EXPECT_EQ(fragment_from_a.size(), same_fragment.size());

  Fragment fragment_from_b = memory_b().AllocateFragment(16);
  EXPECT_TRUE(fragment_from_b.is_addressable());
  EXPECT_EQ(BufferId(0), fragment_from_b.buffer_id());
  EXPECT_GE(fragment_from_b.size(), 16u);

  same_fragment = memory_a().GetFragment(fragment_from_b.descriptor());
  EXPECT_TRUE(same_fragment.is_addressable());
  EXPECT_EQ(fragment_from_b.buffer_id(), same_fragment.buffer_id());
  EXPECT_EQ(fragment_from_b.offset(), same_fragment.offset());
  EXPECT_EQ(fragment_from_b.size(), same_fragment.size());

  EXPECT_TRUE(memory_a().FreeFragment(fragment_from_b));
  EXPECT_TRUE(memory_b().FreeFragment(fragment_from_a));
}

TEST_F(NodeLinkMemoryTest, ExpandCapacity) {
  // If we depelete a NodeLinkMemory's capacity to allocate fragments of a given
  // size, it should automatically acquire new capacity for future allocations.

  constexpr size_t kSize = 64;
  bool has_new_capacity = false;
  memory_a().WaitForBufferAsync(
      BufferId(1), [&has_new_capacity] { has_new_capacity = true; });
  while (!memory_a().AllocateFragment(kSize).is_null())
    ;

  // Since we're using a synchronous driver, this should have already been true
  // by the time the most recent failed allocation returned.
  EXPECT_TRUE(has_new_capacity);

  // And a subsequent allocation request should now succeed with a fragment from
  // the new buffer.
  Fragment fragment = memory_a().AllocateFragment(kSize);
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(BufferId(1), fragment.buffer_id());

  // The new buffer should have also been shared with the other NodeLinkMemory
  // already.
  EXPECT_TRUE(memory_b().FreeFragment(fragment));
}

TEST_F(NodeLinkMemoryTest, LimitedCapacityExpansion) {
  // A NodeLinkMemory will eventually stop expanding its capacity for new
  // fragments of a given size.
  static constexpr size_t kSize = 64;
  std::vector<Fragment> fragments;
  auto try_alloc = [&fragments, this] {
    Fragment fragment = memory_a().AllocateFragment(kSize);
    if (!fragment.is_null()) {
      fragments.push_back(fragment);
    }
    return !fragment.is_null();
  };

  do {
    // Deplete the current capacity.
    while (try_alloc()) {
    }

    // Because we're using a synchronous driver, if the NodeLinkMemory will
    // expand its capacity at all, it will have already done so by the time the
    // the failed allocation returns above. So if allocation fails again here,
    // then we've reached the capacity limit for this fragment size and we can
    // end the test.
  } while (try_alloc());

  // Any additionally allocated buffers should already have been shared with the
  // other NodeLinkMemory. Let it free all of the fragments and verify success
  // in every case.
  for (const auto& fragment : fragments) {
    EXPECT_TRUE(memory_b().FreeFragment(fragment));
  }
}

TEST_F(NodeLinkMemoryTest, OversizedAllocation) {
  // Allocations which are too large for block-based allocation will fail for
  // now. This may change as new allocation schemes are supported.
  constexpr size_t kWayTooBig = 64 * 1024 * 1024;
  Fragment fragment = memory_a().AllocateFragment(kWayTooBig);
  EXPECT_TRUE(fragment.is_null());
}

TEST_F(NodeLinkMemoryTest, NewBlockSizes) {
  // NodeLinkMemory begins life with a fixed set of block allocators available
  // for certain common block sizes. These are capped out at 2 kB blocks, but
  // NodeLinkMemory still supports block allocation of larger blocks as well --
  // at least up to 16 kB in size. Verify that we can trigger new capacity for
  // such sizes by attempting to allocate them.

  constexpr size_t kPrettyBig = 16 * 1024;
  Fragment fragment = memory_a().AllocateFragment(kPrettyBig);

  // No initial capacity for 16 kB fragments.
  EXPECT_TRUE(fragment.is_null());

  // But the failure above should have triggered expansion of capacity for that
  // size. This request should succeed.
  fragment = memory_a().AllocateFragment(kPrettyBig);
  EXPECT_FALSE(fragment.is_null());
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_GE(fragment.size(), kPrettyBig);

  // And as with other cases, the new capacity should have already been shared
  // with the other NodeLinkMemory.
  EXPECT_TRUE(memory_b().FreeFragment(fragment));
}

}  // namespace
}  // namespace ipcz
