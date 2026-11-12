// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_link_memory.h"

#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/features.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/metrics.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_name.h"
#include "ipcz/parcel.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test/mock_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& GetTestDriver() {
  return reference_drivers::GetSyncReferenceDriver();
}

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
    auto transports = DriverTransport::CreatePair(GetTestDriver());
    DriverMemoryWithMapping buffer =
        NodeLinkMemory::AllocateMemory(GetTestDriver());
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

  // Creates inactive links between node_a() and `node_c`, registered with both
  // nodes, and returns node_a()'s link followed by `node_c`'s link.
  std::pair<Ref<NodeLink>, Ref<NodeLink>> CreateInactiveLinks(
      const Ref<Node>& node_c) {
    auto transports = DriverTransport::CreatePair(GetTestDriver());
    DriverMemoryWithMapping buffer =
        NodeLinkMemory::AllocateMemory(GetTestDriver());
    Ref<NodeLink> link_a = NodeLink::CreateInactive(
        node_a(), LinkSide::kA, node_a()->GetAssignedName(),
        kOtherTestNonBrokerName, Node::Type::kNormal, 0, Features{},
        transports.first,
        NodeLinkMemory::Create(node_a(), LinkSide::kA, Features{},
                               std::move(buffer.mapping)));
    Ref<NodeLink> link_c = NodeLink::CreateInactive(
        node_c, LinkSide::kB, kOtherTestNonBrokerName,
        node_a()->GetAssignedName(), Node::Type::kBroker, 0, Features{},
        transports.second,
        NodeLinkMemory::Create(node_c, LinkSide::kB, Features{},
                               buffer.memory.Map()));
    node_a()->AddConnection(kOtherTestNonBrokerName, {.link = link_a});
    node_c->AddConnection(node_a()->GetAssignedName(),
                          {.link = link_c, .broker = link_c});
    return {std::move(link_a), std::move(link_c)};
  }

  // Connects node_a() to a new non-broker node using `flags` as its memory
  // flags. The new node is closed on TearDown(). Returns node_a()'s link
  // followed by the new node's. Call at most once per test.
  std::pair<Ref<NodeLink>, Ref<NodeLink>> ConnectNodeWithMemoryFlags(
      IpczMemoryFlags flags) {
    ABSL_ASSERT(!node_with_memory_flags_);
    // Node copies its options, so they need not outlive this call.
    const IpczCreateNodeOptions options = {
        .size = sizeof(options),
        .memory_flags = flags,
    };
    node_with_memory_flags_ =
        MakeRefCounted<Node>(Node::Type::kNormal, GetTestDriver(), &options);
    return ConnectNodes(node_a(), node_with_memory_flags_,
                        kOtherTestNonBrokerName);
  }

  static void AddBlocksToMemory(NodeLinkMemory& memory, size_t block_size) {
    constexpr size_t kNumBlocks = 32;
    auto mapping = DriverMemory(GetTestDriver(), block_size * kNumBlocks).Map();

    BlockAllocator allocator(mapping.bytes(),
                             static_cast<uint32_t>(block_size));
    allocator.InitializeRegion();

    const BufferId id = memory.AllocateNewBufferId();
    memory.AddBlockBuffer(id, block_size, std::move(mapping));
  }

  static std::vector<Fragment> DrainFragments(NodeLinkMemory& memory,
                                              size_t size) {
    std::vector<Fragment> allocated;
    for (;;) {
      Fragment fragment = memory.AllocateFragment(size);
      if (fragment.is_null()) {
        break;
      }
      allocated.push_back(fragment);
    }
    return allocated;
  }

  // Expects every block of `block_size` bytes in `memory` to be allocable
  // before an allocation fails, followed by `num_expansions` capacity
  // expansions of one new buffer each and no more. With the synchronous driver,
  // each expansion completes within the failed allocation which triggers it.
  static void ExpectParcelExpansions(NodeLinkMemory& memory,
                                     size_t block_size,
                                     size_t num_expansions) {
    SCOPED_TRACE(testing::Message() << "block_size=" << block_size);
    // Read the initial block count before draining, which may expand it.
    const size_t initial_blocks = memory.TotalBlockCountForTesting(block_size);
    EXPECT_EQ(initial_blocks, DrainFragments(memory, block_size).size());
    const size_t blocks_per_expansion =
        NodeLinkMemory::NewBufferBlockCountForTesting(block_size);
    for (size_t i = 0; i < num_expansions; ++i) {
      EXPECT_EQ(blocks_per_expansion, DrainFragments(memory, block_size).size())
          << "expansion " << i;
    }
    EXPECT_TRUE(memory.AllocateFragment(block_size).is_null());
  }

  // Exhausts the 64-byte capacity of `memory` with RouterLinkStates, then
  // expects the next one to come from a new buffer: RouterLinkState allocation
  // may expand capacity under any parcel capacity policy.
  static void ExpectRouterLinkStateCapacityExpands(NodeLinkMemory& memory) {
    // Keep each state alive so that its block stays allocated.
    std::vector<FragmentRef<RouterLinkState>> states;
    for (;;) {
      FragmentRef<RouterLinkState> state = memory.TryAllocateRouterLinkState();
      if (state.is_null()) {
        break;
      }
      states.push_back(std::move(state));
    }

    const FragmentRef<RouterLinkState> state =
        memory.TryAllocateRouterLinkState();
    ASSERT_FALSE(state.is_null());
    EXPECT_NE(NodeLinkMemory::kPrimaryBufferId, state.fragment().buffer_id());
  }

  void SetUp() override {
    // Brokers assign their own names, no need to assign one to `node_a_`.
    auto links = ConnectNodes(node_a_, node_b_, kTestNonBrokerName);
    link_a_ = std::move(links.first);
    link_b_ = std::move(links.second);
  }

  void TearDown() override {
    if (node_with_memory_flags_) {
      node_with_memory_flags_->Close();
    }
    node_b_->Close();
    node_a_->Close();
  }

 private:
  const Ref<Node> node_a_{
      MakeRefCounted<Node>(Node::Type::kBroker, GetTestDriver())};
  const Ref<Node> node_b_{
      MakeRefCounted<Node>(Node::Type::kNormal, GetTestDriver())};
  Ref<NodeLink> link_a_;
  Ref<NodeLink> link_b_;

  // Set by ConnectNodeWithMemoryFlags().
  Ref<Node> node_with_memory_flags_;
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

TEST_F(NodeLinkMemoryTest, PreActivationCapacityExpansion) {
  const Ref<Node> node_c{
      MakeRefCounted<Node>(Node::Type::kNormal, GetTestDriver())};
  auto [link_a, link_c] = CreateInactiveLinks(node_c);

  // Drain all 64-byte blocks in PrimaryBuffer and request an 8 KB fragment
  // (which has no pre-allocated capacity in PrimaryBuffer) before the links are
  // activated.
  const size_t num_blocks = link_a->memory().TotalBlockCountForTesting(64);
  EXPECT_EQ(num_blocks, DrainFragments(link_a->memory(), 64).size());
  EXPECT_TRUE(link_a->memory().AllocateFragment(64).is_null());
  EXPECT_TRUE(link_a->memory().AllocateFragment(8192).is_null());

  // A RouterLinkState requested now joins the pending 64-byte request.
  FragmentRef<RouterLinkState> state;
  link_a->memory().AllocateRouterLinkState(
      [&state](FragmentRef<RouterLinkState> new_state) {
        state = std::move(new_state);
      });
  EXPECT_TRUE(state.is_null());

  // Activating the links must fulfill all pending pre-activation capacity
  // requests via SetNodeLink().
  link_a->Activate();
  link_c->Activate();
  EXPECT_TRUE(state.is_addressable());

  Fragment fragment_64 = link_a->memory().AllocateFragment(64);
  ASSERT_TRUE(fragment_64.is_addressable());
  EXPECT_NE(NodeLinkMemory::kPrimaryBufferId, fragment_64.buffer_id());
  EXPECT_TRUE(link_c->memory().FreeFragment(fragment_64));

  Fragment fragment_8k = link_a->memory().AllocateFragment(8192);
  ASSERT_TRUE(fragment_8k.is_addressable());
  EXPECT_EQ(8192u, fragment_8k.size());
  EXPECT_NE(NodeLinkMemory::kPrimaryBufferId, fragment_8k.buffer_id());
  EXPECT_TRUE(link_c->memory().FreeFragment(fragment_8k));

  // Activation added exactly one buffer of 1023 usable 64-byte blocks, one of
  // which `state` holds.
  EXPECT_EQ(1022u, DrainFragments(link_a->memory(), 64).size());

  node_c->Close();
}

// Parcel allocation failures for a block size that already has a capacity
// request in flight join that request rather than starting a new one. Without
// this coalescing, each of the three failures below would add its own 8 KB
// buffer, for 21 new blocks instead of 7.
TEST_F(NodeLinkMemoryTest, InFlightParcelRequestsCoalesce) {
  const Ref<Node> node_c{
      MakeRefCounted<Node>(Node::Type::kNormal, GetTestDriver())};
  auto [link_a, link_c] = CreateInactiveLinks(node_c);
  NodeLinkMemory& memory = link_c->memory();

  // Node C delegates shared memory allocation to node A. While only node C's
  // end of the link is active, its requests to node A remain in flight.
  node_c->SetAllocationDelegate(link_c);
  link_c->Activate();

  // Repeated parcel allocation failures while a request is in flight do not
  // start new requests.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(memory.AllocateFragment(8192).is_null());
  }

  // Activating node A's end completes the single in-flight request, adding one
  // new 8 KB buffer (7 usable blocks).
  link_a->Activate();
  EXPECT_EQ(7u, DrainFragments(memory, 8192).size());

  node_c->Close();
}

TEST_F(NodeLinkMemoryTest, FailedParcelCapacityRequestIsRetryable) {
  // Fail the first shared memory allocation and forward the rest to the
  // reference driver, so node A can still map node C's buffers.
  testing::StrictMock<test::MockDriver> driver;
  EXPECT_CALL(driver, Close).WillRepeatedly([](auto... args) {
    return GetTestDriver().Close(args...);
  });
  EXPECT_CALL(driver, Serialize).WillRepeatedly([](auto... args) {
    return GetTestDriver().Serialize(args...);
  });
  EXPECT_CALL(driver, MapSharedMemory).WillRepeatedly([](auto... args) {
    return GetTestDriver().MapSharedMemory(args...);
  });
  EXPECT_CALL(driver, AllocateSharedMemory)
      .WillOnce(testing::Return(IPCZ_RESULT_RESOURCE_EXHAUSTED))
      .WillRepeatedly([](auto... args) {
        return GetTestDriver().AllocateSharedMemory(args...);
      });
  const Ref<Node> node_c{
      MakeRefCounted<Node>(Node::Type::kNormal, test::GetMockDriver())};
  auto [link_a, link_c] =
      ConnectNodes(node_a(), node_c, kOtherTestNonBrokerName);
  NodeLinkMemory& memory = link_c->memory();

  // The failed 8 KB allocation requests new capacity, but the driver rejects
  // the shared memory allocation.
  EXPECT_TRUE(memory.AllocateFragment(8192).is_null());

  // The failed request does not block new requests, so the next failed
  // allocation requests capacity again and succeeds.
  EXPECT_TRUE(memory.AllocateFragment(8192).is_null());
  EXPECT_EQ(7u, DrainFragments(memory, 8192).size());

  node_c->Close();
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
      MakeRefCounted<Node>(Node::Type::kNormal, GetTestDriver(), &options)};
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

// In the tests below, block sizes go from largest to smallest, because
// BufferPool serves a block size with no capacity of its own from the next
// larger size, which must already be exhausted.

TEST_F(NodeLinkMemoryTest, FixedParcelCapacity) {
  auto [link_a, link_c] =
      ConnectNodeWithMemoryFlags(IPCZ_MEMORY_FIXED_PARCEL_CAPACITY);
  NodeLinkMemory& memory = link_c->memory();

  // Parcel data never expands capacity, but RouterLinkStates still do.
  ExpectParcelExpansions(memory, 8192, 0);
  ExpectParcelExpansions(memory, 64, 0);
  ExpectRouterLinkStateCapacityExpands(memory);
}

TEST_F(NodeLinkMemoryTest, NoParcelCapacity) {
  auto [link_a, link_c] =
      ConnectNodeWithMemoryFlags(IPCZ_MEMORY_NO_PARCEL_CAPACITY);
  NodeLinkMemory& memory = link_c->memory();

  // Parcel data never uses shared memory, even though PrimaryBuffer has free
  // blocks of the needed sizes.
  Parcel parcel;
  parcel.AllocateData(32, /*allow_partial=*/false, &memory);
  EXPECT_FALSE(parcel.has_data_fragment());
  EXPECT_TRUE(memory.AllocateFragment(64).is_null());
  EXPECT_TRUE(memory.AllocateFragmentBestEffort(4096).is_null());

  // RouterLinkStates still use and expand 64-byte capacity, which parcel data
  // still cannot use.
  ExpectRouterLinkStateCapacityExpands(memory);
  Parcel parcel_after_expansion;
  parcel_after_expansion.AllocateData(32, /*allow_partial=*/false, &memory);
  EXPECT_FALSE(parcel_after_expansion.has_data_fragment());
}

TEST_F(NodeLinkMemoryTest, ExpandParcelCapacityOnce) {
  auto [link_a, link_c] =
      ConnectNodeWithMemoryFlags(IPCZ_MEMORY_EXPAND_PARCEL_CAPACITY_ONCE);
  NodeLinkMemory& memory = link_c->memory();

  // Block sizes up to 32 KB expand once, and only while the link's capacity for
  // them is below what one new buffer adds. So 64-byte blocks never expand:
  // PrimaryBuffer alone already has more of them than one new buffer adds.
  ASSERT_GT(memory.TotalBlockCountForTesting(64),
            NodeLinkMemory::NewBufferBlockCountForTesting(64));
  ExpectParcelExpansions(memory, 64 * 1024, 0);
  ExpectParcelExpansions(memory, 32 * 1024, 1);
  ExpectParcelExpansions(memory, 16 * 1024, 1);
  ExpectParcelExpansions(memory, 8 * 1024, 1);
  ExpectParcelExpansions(memory, 4096, 1);
  ExpectParcelExpansions(memory, 2048, 1);
  ExpectParcelExpansions(memory, 1024, 1);
  ExpectParcelExpansions(memory, 512, 1);
  ExpectParcelExpansions(memory, 256, 1);
  ExpectParcelExpansions(memory, 128, 1);
  ExpectParcelExpansions(memory, 64, 0);
  ExpectRouterLinkStateCapacityExpands(memory);
}

TEST_F(NodeLinkMemoryTest, ExpandParcelCapacityOnceCountsRemoteExpansion) {
  auto [link_a, link_c] =
      ConnectNodeWithMemoryFlags(IPCZ_MEMORY_EXPAND_PARCEL_CAPACITY_ONCE);

  // Node A uses the default policy, so draining the 256-byte blocks in
  // PrimaryBuffer from its side adds a buffer of 256-byte blocks to the link's
  // shared pool.
  const size_t num_blocks = link_a->memory().TotalBlockCountForTesting(256);
  EXPECT_EQ(num_blocks, DrainFragments(link_a->memory(), 256).size());

  // Node C can allocate from the buffer added by node A, but since the link's
  // pool already has a dynamic buffer for 256-byte blocks, it never adds one.
  EXPECT_EQ(NodeLinkMemory::NewBufferBlockCountForTesting(256),
            DrainFragments(link_c->memory(), 256).size());
  EXPECT_TRUE(link_c->memory().AllocateFragment(256).is_null());
}

TEST_F(NodeLinkMemoryTest, RestrictedDynamicParcelCapacity) {
  auto [link_a, link_c] = ConnectNodeWithMemoryFlags(
      IPCZ_MEMORY_RESTRICTED_DYNAMIC_PARCEL_CAPACITY);
  NodeLinkMemory& memory = link_c->memory();

  // Block sizes expand as long as their usable capacity stays within 512 KB.
  // Block sizes above 64 KB therefore never expand, because a single new buffer
  // for them already exceeds 512 KB.
  ASSERT_GT(
      128 * 1024 * NodeLinkMemory::NewBufferBlockCountForTesting(128 * 1024),
      512u * 1024);
  ExpectParcelExpansions(memory, 128 * 1024, 0);
  ExpectParcelExpansions(memory, 64 * 1024, 1);
  ExpectParcelExpansions(memory, 32 * 1024, 2);
  ExpectParcelExpansions(memory, 16 * 1024, 4);
  ExpectParcelExpansions(memory, 8 * 1024, 9);
  ExpectParcelExpansions(memory, 4096, 8);
  ExpectParcelExpansions(memory, 2048, 8);
  ExpectParcelExpansions(memory, 1024, 8);
  ExpectParcelExpansions(memory, 512, 8);
  ExpectParcelExpansions(memory, 256, 8);
  ExpectParcelExpansions(memory, 128, 8);
  ExpectParcelExpansions(memory, 64, 6);
  ExpectRouterLinkStateCapacityExpands(memory);
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

TEST_F(NodeLinkMemoryTest, AllocateFragmentHistogram) {
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;

  // Successful allocation (64 bytes).
  Fragment fragment = memory_a().AllocateFragment(64);
  EXPECT_TRUE(fragment.is_addressable());
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.BufferPoolAllocateBlockResult",
                                     /*sample=*/true, 1);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.BufferPoolAllocateBlockSuccessSize2",
      static_cast<int>(metrics::BlockAllocationSize::k64B), 1);

  // Small requests (<= kMaxFragmentSizeForBlockAllocation) go through the
  // BufferPool, so they land in both sets of metrics.
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.ParcelAllocateBlockResult",
                                     /*sample=*/true, 1);

  // Failed allocation for an unregistered size (8192 bytes).
  Fragment failed = memory_a().AllocateFragment(8192);
  EXPECT_TRUE(failed.is_null());
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.BufferPoolAllocateBlockResult",
                                     /*sample=*/false, 1);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.BufferPoolAllocateBlockFailureSize2",
      static_cast<int>(metrics::BlockAllocationSize::k8KB), 1);
}

TEST_F(NodeLinkMemoryTest, AllocateRouterLinkStateHistogram) {
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;

  // AllocateRouterLinkState.
  bool callback_run = false;
  memory_a().AllocateRouterLinkState(
      [&](FragmentRef<RouterLinkState> async_state) {
        EXPECT_TRUE(async_state.is_addressable());
        callback_run = true;
      });
  EXPECT_TRUE(callback_run);
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.BufferPoolAllocateBlockResult",
                                     /*sample=*/true, 1);
  constexpr int kBlockAllocationSize64Bytes =
      static_cast<int>(metrics::BlockAllocationSize::k64B);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.BufferPoolAllocateBlockSuccessSize2",
      /*sample=*/kBlockAllocationSize64Bytes, 1);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.BufferPoolAllocateBlockFailureSize2",
      /*sample=*/kBlockAllocationSize64Bytes, 0);

  // RouterLinkState allocation goes straight to the BufferPool, so it must not
  // show up in the parcel metrics.
  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockResult", 0);
}

TEST_F(NodeLinkMemoryTest, ParcelAllocateBlockHistogram) {
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;

  // A zero-sized request is not a meaningful allocation attempt.
  EXPECT_TRUE(memory_a().AllocateFragment(0).is_null());
  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockResult", 0);

  // Successful parcel shared memory allocation (50 bytes plus a FragmentHeader
  // fits in a 64B block).
  Parcel parcel1;
  parcel1.AllocateData(/*num_bytes=*/50, /*allow_partial=*/false, &memory_a());
  EXPECT_TRUE(parcel1.has_data_fragment());
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.ParcelAllocateBlockResult",
                                     /*sample=*/true, 1);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.ParcelAllocateBlockSuccessSize",
      static_cast<int>(metrics::BlockAllocationSize::k64B), 1);

  // Best-effort parcel shared memory allocation (allow_partial=true) should NOT
  // record to parcel histograms.
  Parcel parcel_best_effort;
  parcel_best_effort.AllocateData(/*num_bytes=*/4000, /*allow_partial=*/true,
                                  &memory_a());
  EXPECT_TRUE(parcel_best_effort.has_data_fragment());
  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockResult", 1);

  // Failed parcel shared memory allocation for unregistered size (8192 bytes
  // plus a FragmentHeader requires a 16KB block).
  Parcel parcel2;
  parcel2.AllocateData(/*num_bytes=*/8192, /*allow_partial=*/false,
                       &memory_a());
  EXPECT_FALSE(parcel2.has_data_fragment());
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.ParcelAllocateBlockResult",
                                     /*sample=*/false, 1);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.ParcelAllocateBlockFailureSize",
      static_cast<int>(metrics::BlockAllocationSize::k16KB), 1);

  // Failed allocation for size exceeding 1MB (2MB maps to kOther). The request
  // never reaches the BufferPool, so it records no BufferPool sample.
  const auto buffer_pool_samples =
      histogram_tester.GetAllSamples("Mojo.Ipcz.BufferPoolAllocateBlockResult");
  EXPECT_FALSE(buffer_pool_samples.empty());
  Parcel parcel3;
  parcel3.AllocateData(/*num_bytes=*/2 * 1024 * 1024, /*allow_partial=*/false,
                       &memory_a());
  EXPECT_FALSE(parcel3.has_data_fragment());
  EXPECT_EQ(buffer_pool_samples,
            histogram_tester.GetAllSamples(
                "Mojo.Ipcz.BufferPoolAllocateBlockResult"));
  histogram_tester.ExpectBucketCount("Mojo.Ipcz.ParcelAllocateBlockResult",
                                     /*sample=*/false, 2);
  histogram_tester.ExpectBucketCount(
      "Mojo.Ipcz.ParcelAllocateBlockFailureSize",
      static_cast<int>(metrics::BlockAllocationSize::kOther), 1);

  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockResult", 3);
  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockSuccessSize",
                                    1);
  histogram_tester.ExpectTotalCount("Mojo.Ipcz.ParcelAllocateBlockFailureSize",
                                    2);
}

}  // namespace
}  // namespace ipcz
