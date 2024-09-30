// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link_memory.h"

#include <utility>
#include <vector>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/features.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_name.h"
#include "ipcz/parcel.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

constexpr NodeName kTestNonBrokerName(2, 3);
constexpr NodeName kOtherTestNonBrokerName(3, 5);

class NodeLinkMemoryTest : public testing::Test {
 public:
  const Ref<Node>& node_a() const { return node_a_; }

  NodeLinkMemory& memory_a() { return link_a_->memory(); }
  NodeLinkMemory& memory_b() { return link_b_->memory(); }

  // Connects a broker to a non-broker and returns their respective NodeLinks.
  static std::pair<Ref<NodeLink>, Ref<NodeLink>> ConnectNodes(
      Ref<Node> broker,
      Ref<Node> non_broker,
      const NodeName& non_broker_name) {
    std::pair<Ref<NodeLink>, Ref<NodeLink>> links;
    auto transports = DriverTransport::CreatePair(kTestDriver);
    DriverMemoryWithMapping buffer =
        NodeLinkMemory::AllocateMemory(kTestDriver);
    links.first = NodeLink::CreateInactive(
        broker, LinkSide::kA, broker->GetAssignedName(), non_broker_name,
        Node::Type::kNormal, 0, Features{}, transports.first,
        NodeLinkMemory::Create(broker, LinkSide::kA, Features{},
                               std::move(buffer.mapping)));
    links.second = NodeLink::CreateInactive(
        non_broker, LinkSide::kB, non_broker_name, broker->GetAssignedName(),
        Node::Type::kBroker, 0, Features{}, transports.second,
        NodeLinkMemory::Create(non_broker, LinkSide::kB, Features{},
                               buffer.memory.Map()));
    broker->AddConnection(non_broker_name, {.link = links.first});
    non_broker->AddConnection(broker->GetAssignedName(),
                              {.link = links.second, .broker = links.first});
    links.first->Activate();
    links.second->Activate();
    return links;
  }

  static void AddBlocksToMemory(NodeLinkMemory& memory, size_t block_size) {
    constexpr size_t kNumBlocks = 32;
    auto mapping = DriverMemory(kTestDriver, block_size * kNumBlocks).Map();

    BlockAllocator allocator(mapping.bytes(),
                             static_cast<uint32_t>(block_size));
    allocator.InitializeRegion();

    const BufferId id = memory.AllocateNewBufferId();
    memory.AddBlockBuffer(id, block_size, std::move(mapping));
  }

  void SetUp() override {
    // Brokers assign their own names, no need to assign one to `node_a_`.
    auto links = ConnectNodes(node_a_, node_b_, kTestNonBrokerName);
    link_a_ = std::move(links.first);
    link_b_ = std::move(links.second);
  }

  void TearDown() override {
    node_b_->Close();
    node_a_->Close();
  }

 private:
  const Ref<Node> node_a_{
      MakeRefCounted<Node>(Node::Type::kBroker, kTestDriver)};
  const Ref<Node> node_b_{
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver)};
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
  AddBlocksToMemory(memory_a(), /*block_size=*/256);
  AddBlocksToMemory(memory_a(), /*block_size=*/512);

  // Fragment sizes are rounded up to the nearest power of 2.
  Fragment fragment = memory_a().AllocateFragment(32);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(64u, fragment.size());

  fragment = memory_a().AllocateFragment(250);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(256u, fragment.size());

  fragment = memory_a().AllocateFragment(257);
  EXPECT_TRUE(fragment.is_addressable());
  EXPECT_EQ(512u, fragment.size());
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
  // for certain common block sizes. These are capped out at 64 kB blocks, but
  // NodeLinkMemory still supports block allocation of larger blocks as well --
  // at least up to 1 MB in size. Verify that we can trigger new capacity for
  // such sizes by attempting to allocate them.

  constexpr size_t kPrettyBig = 512 * 1024;
  Fragment fragment = memory_a().AllocateFragment(kPrettyBig);

  // No initial capacity for 256 kB fragments.
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

TEST_F(NodeLinkMemoryTest, ParcelDataAllocation) {
  // NodeLinkMemory can in general be used by Parcel instances to allocate data
  // buffers, but dynamic expansion of the allocation capacity can be disabled
  // when configuring a new node.

  const IpczCreateNodeOptions options = {
      .size = sizeof(options),
      .memory_flags = IPCZ_MEMORY_FIXED_PARCEL_CAPACITY,
  };
  const Ref<Node> node_c{
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver, &options)};
  auto links = ConnectNodes(node_a(), node_c, kOtherTestNonBrokerName);

  // We use a small enough size that this is guaranteed to allocate within
  // NodeLinkMemory. But we allocate them from node C's side of the link, where
  // capacity expansion is disabled. This loop should therefore eventually
  // terminate. Since we're using a synchronous test driver, if the memory were
  // going to expand its capacity at all, it would do so synchronously within
  // AllocateData.
  constexpr size_t kParcelSize = 32;
  std::vector<std::unique_ptr<Parcel>> parcels;
  for (;;) {
    auto parcel = std::make_unique<Parcel>();
    parcel->AllocateData(kParcelSize, /*allow_partial=*/false,
                         &links.second->memory());
    if (!parcel->has_data_fragment()) {
      break;
    }

    // Every fragment allocated must be of sufficient size and must be in the
    // link memory's primary buffer ONLY.
    EXPECT_GE(parcel->data_fragment().size(), kParcelSize);
    EXPECT_EQ(NodeLinkMemory::kPrimaryBufferId,
              parcel->data_fragment().buffer_id());
    parcels.push_back(std::move(parcel));
  }

  EXPECT_FALSE(parcels.empty());
  node_c->Close();
}

struct TestObject : public RefCountedFragment {
 public:
  int x;
  int y;
};

TEST_F(NodeLinkMemoryTest, AdoptFragmentRefIfValid) {
  auto object = memory_a().AdoptFragmentRef<TestObject>(
      memory_a().AllocateFragment(sizeof(TestObject)));
  object->x = 5;
  object->y = 42;

  const FragmentDescriptor valid_descriptor(object.fragment().buffer_id(),
                                            object.fragment().offset(),
                                            sizeof(TestObject));

  const FragmentDescriptor null_descriptor(
      kInvalidBufferId, valid_descriptor.offset(), valid_descriptor.size());
  EXPECT_TRUE(memory_a()
                  .AdoptFragmentRefIfValid<TestObject>(null_descriptor)
                  .is_null());

  const FragmentDescriptor empty_descriptor(
      valid_descriptor.buffer_id(), valid_descriptor.offset(), /*size=*/0);
  EXPECT_TRUE(memory_a()
                  .AdoptFragmentRefIfValid<TestObject>(empty_descriptor)
                  .is_null());

  const FragmentDescriptor short_descriptor(valid_descriptor.buffer_id(),
                                            valid_descriptor.offset(),
                                            sizeof(TestObject) - 4);
  EXPECT_TRUE(memory_a()
                  .AdoptFragmentRefIfValid<TestObject>(short_descriptor)
                  .is_null());

  const FragmentDescriptor unaligned_descriptor(valid_descriptor.buffer_id(),
                                                valid_descriptor.offset() + 2,
                                                valid_descriptor.size() - 2);
  EXPECT_TRUE(memory_a()
                  .AdoptFragmentRefIfValid<TestObject>(unaligned_descriptor)
                  .is_null());

  const auto adopted_object =
      memory_a().AdoptFragmentRefIfValid<TestObject>(valid_descriptor);
  ASSERT_TRUE(adopted_object.is_addressable());
  EXPECT_EQ(5, adopted_object->x);
  EXPECT_EQ(42, adopted_object->y);
}

}  // namespace
}  // namespace ipcz
