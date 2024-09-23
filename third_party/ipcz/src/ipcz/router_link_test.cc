// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router_link.h"

#include <tuple>
#include <utility>
#include <vector>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/features.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/local_router_link.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "ipcz/node_name.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/router.h"
#include "ipcz/router_link_state.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

enum class RouterLinkTestMode {
  kLocal,
  kRemote,
};

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

constexpr NodeName kTestBrokerName(1, 2);
constexpr NodeName kTestNonBrokerName(2, 3);
constexpr NodeName kTestPeer1Name(3, 4);
constexpr NodeName kTestPeer2Name(4, 5);

// A helper for the tests in this module, TestNodePair creates one broker node
// and one non-broker node and interconnects them using the synchronous
// reference driver. This class exposes the NodeLinkMemory on either end of the
// connection and provides some additional facilities which tests can use to
// poke at node and router state.
class TestNodePair {
 public:
  TestNodePair() {
    auto transports = DriverTransport::CreatePair(kTestDriver);
    DriverMemoryWithMapping buffer =
        NodeLinkMemory::AllocateMemory(kTestDriver);
    node_link_a_ = NodeLink::CreateInactive(
        node_a_, LinkSide::kA, kTestBrokerName, kTestNonBrokerName,
        Node::Type::kNormal, 0, Features{}, transports.first,
        NodeLinkMemory::Create(node_a_, LinkSide::kA, Features{},
                               std::move(buffer.mapping)));
    node_link_b_ = NodeLink::CreateInactive(
        node_b_, LinkSide::kB, kTestNonBrokerName, kTestBrokerName,
        Node::Type::kBroker, 0, Features{}, transports.second,
        NodeLinkMemory::Create(node_b_, LinkSide::kB, Features{},
                               buffer.memory.Map()));
    node_a_->AddConnection(kTestNonBrokerName, {.link = node_link_a_});
    node_b_->AddConnection(kTestBrokerName,
                           {.link = node_link_b_, .broker = node_link_b_});
  }

  ~TestNodePair() {
    node_b_->Close();
    node_a_->Close();
  }

  NodeLinkMemory& memory_a() const { return node_link_a_->memory(); }
  NodeLinkMemory& memory_b() const { return node_link_b_->memory(); }

  // Activates both of the test nodes' NodeLink transports. Tests can defer this
  // activation as a means of deferring NodeLink communications in general.
  void ActivateTransports() {
    node_link_a_->Activate();
    node_link_b_->Activate();
  }

  // Establishes new RemoteRouterLinks between `a` and `b`. Different initial
  // RouterLinkState references may be provided for the link on either side in
  // order to mimic various production scenarios.
  RouterLink::Pair LinkRemoteRouters(Ref<Router> a,
                                     FragmentRef<RouterLinkState> a_state,
                                     Ref<Router> b,
                                     FragmentRef<RouterLinkState> b_state) {
    const SublinkId sublink = node_link_a_->memory().AllocateSublinkIds(1);
    Ref<RemoteRouterLink> a_link = node_link_a_->AddRemoteRouterLink(
        sublink, std::move(a_state), LinkType::kCentral, LinkSide::kA, a);
    Ref<RemoteRouterLink> b_link = node_link_b_->AddRemoteRouterLink(
        sublink, std::move(b_state), LinkType::kCentral, LinkSide::kB, b);
    a->SetOutwardLink(a_link);
    b->SetOutwardLink(b_link);
    return {a_link, b_link};
  }

  // Depletes the available supply of RouterLinkState fragments and returns
  // references to all of them. Note that one side effect of this call is that
  // memory_a() will expand its RouterLinkState fragment capacity, so subsequent
  // allocation requests will still succeed.
  std::vector<FragmentRef<RouterLinkState>> AllocateAllRouterLinkStates() {
    std::vector<FragmentRef<RouterLinkState>> fragments;
    for (;;) {
      FragmentRef<RouterLinkState> fragment =
          memory_a().TryAllocateRouterLinkState();
      if (fragment.is_null()) {
        return fragments;
      }
      fragments.push_back(std::move(fragment));
    }
  }

 private:
  const Ref<Node> node_a_{
      MakeRefCounted<Node>(Node::Type::kBroker, kTestDriver)};
  const Ref<Node> node_b_{
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver)};
  Ref<NodeLink> node_link_a_;
  Ref<NodeLink> node_link_b_;
};

class RouterLinkTest : public testing::Test,
                       public testing::WithParamInterface<RouterLinkTestMode> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case RouterLinkTestMode::kLocal:
        std::tie(a_link_, b_link_) =
            LocalRouterLink::CreatePair(LinkType::kCentral, {a_, b_});
        a_->SetOutwardLink(a_link_);
        b_->SetOutwardLink(b_link_);
        break;

      case RouterLinkTestMode::kRemote: {
        auto link_state = nodes_.memory_a().TryAllocateRouterLinkState();
        ABSL_ASSERT(link_state.is_addressable());
        link_state_ = link_state.get();
        std::tie(a_link_, b_link_) =
            nodes_.LinkRemoteRouters(a_, link_state, b_, link_state);
        break;
      }
    }

    ASSERT_EQ(a_link_->GetLinkState(), b_link_->GetLinkState());
    link_state_ = a_link_->GetLinkState();

    nodes_.ActivateTransports();
  }

  void TearDown() override {
    a_->CloseRoute();
    b_->CloseRoute();
  }

  Router& a() { return *a_; }
  Router& b() { return *b_; }
  RouterLink& a_link() { return *a_link_; }
  RouterLink& b_link() { return *b_link_; }
  RouterLinkState& link_state() { return *link_state_; }
  RouterLinkState::Status link_status() { return link_state_->status; }

 private:
  TestNodePair nodes_;
  const Ref<Router> a_{MakeRefCounted<Router>()};
  const Ref<Router> b_{MakeRefCounted<Router>()};
  Ref<RouterLink> a_link_;
  Ref<RouterLink> b_link_;
  RouterLinkState* link_state_ = nullptr;
};

TEST_P(RouterLinkTest, Locking) {
  link_state().status = RouterLinkState::kUnstable;

  // No locking can take place until both sides are marked stable.
  EXPECT_FALSE(a_link().TryLockForBypass(kTestPeer1Name));
  EXPECT_FALSE(a_link().TryLockForClosure());
  a_link().MarkSideStable();
  b_link().MarkSideStable();
  EXPECT_EQ(RouterLinkState::kStable, link_status());

  // Only one side can lock for bypass, and (only) the other side can use this
  // to validate the source of a future bypass request.
  EXPECT_FALSE(b_link().CanNodeRequestBypass(kTestPeer1Name));
  EXPECT_TRUE(a_link().TryLockForBypass(kTestPeer1Name));
  EXPECT_FALSE(b_link().TryLockForBypass(kTestPeer2Name));
  EXPECT_FALSE(b_link().CanNodeRequestBypass(kTestPeer2Name));
  EXPECT_TRUE(b_link().CanNodeRequestBypass(kTestPeer1Name));
  EXPECT_FALSE(a_link().CanNodeRequestBypass(kTestPeer1Name));
  EXPECT_EQ(RouterLinkState::kStable | RouterLinkState::kLockedBySideA,
            link_status());
  a_link().Unlock();
  EXPECT_EQ(RouterLinkState::kStable, link_status());

  EXPECT_TRUE(b_link().TryLockForBypass(kTestPeer2Name));
  EXPECT_FALSE(a_link().TryLockForBypass(kTestPeer1Name));
  EXPECT_FALSE(a_link().CanNodeRequestBypass(kTestPeer1Name));
  EXPECT_FALSE(b_link().CanNodeRequestBypass(kTestPeer2Name));
  EXPECT_TRUE(a_link().CanNodeRequestBypass(kTestPeer2Name));
  EXPECT_EQ(RouterLinkState::kStable | RouterLinkState::kLockedBySideB,
            link_status());
  b_link().Unlock();
  EXPECT_EQ(RouterLinkState::kStable, link_status());

  EXPECT_TRUE(a_link().TryLockForClosure());
  EXPECT_FALSE(b_link().TryLockForClosure());
  EXPECT_EQ(RouterLinkState::kStable | RouterLinkState::kLockedBySideA,
            link_status());
  a_link().Unlock();
  EXPECT_EQ(RouterLinkState::kStable, link_status());

  EXPECT_TRUE(b_link().TryLockForClosure());
  EXPECT_FALSE(a_link().TryLockForClosure());
  EXPECT_EQ(RouterLinkState::kStable | RouterLinkState::kLockedBySideB,
            link_status());
  b_link().Unlock();
  EXPECT_EQ(RouterLinkState::kStable, link_status());
}

TEST_P(RouterLinkTest, FlushOtherSideIfWaiting) {
  link_state().status = RouterLinkState::kUnstable;

  // FlushOtherSideIfWaiting() does nothing if the other side is not, in fact,
  // waiting for something.
  EXPECT_FALSE(a_link().FlushOtherSideIfWaiting());
  EXPECT_FALSE(b_link().FlushOtherSideIfWaiting());
  EXPECT_EQ(RouterLinkState::kUnstable, link_status());

  // Mark B stable and try to lock the link. Since A is not yet stable, this
  // should fail and set B's waiting bit.
  b_link().MarkSideStable();
  EXPECT_FALSE(b_link().TryLockForBypass(kTestPeer1Name));
  EXPECT_EQ(RouterLinkState::kSideBStable | RouterLinkState::kSideBWaiting,
            link_status());

  // Now mark A stable. The FlushOtherSideIfWaiting() should successfully
  // flush B and clear its waiting bit.
  a_link().MarkSideStable();
  EXPECT_EQ(RouterLinkState::kStable | RouterLinkState::kSideBWaiting,
            link_status());
  EXPECT_TRUE(a_link().FlushOtherSideIfWaiting());
  EXPECT_EQ(RouterLinkState::kStable, link_status());
}

class RemoteRouterLinkTest : public testing::Test {
 public:
  TestNodePair& nodes() { return nodes_; }

  std::vector<Router::Pair> CreateTestRouterPairs(size_t n) {
    std::vector<Router::Pair> pairs;
    pairs.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      pairs.emplace_back(MakeRefCounted<Router>(), MakeRefCounted<Router>());
    }
    return pairs;
  }

  void CloseRoutes(const std::vector<Router::Pair>& routers) {
    for (const auto& pair : routers) {
      pair.first->CloseRoute();
      pair.second->CloseRoute();
    }
  }

  BufferId GenerateBufferId() {
    return nodes().memory_a().AllocateNewBufferId();
  }

 private:
  TestNodePair nodes_;
};

TEST_F(RemoteRouterLinkTest, NewLinkWithAddressableState) {
  nodes().ActivateTransports();

  std::vector<FragmentRef<RouterLinkState>> fragments =
      nodes().AllocateAllRouterLinkStates();
  std::vector<Router::Pair> router_pairs =
      CreateTestRouterPairs(fragments.size());
  std::vector<RouterLink::Pair> links;
  for (size_t i = 0; i < fragments.size(); ++i) {
    auto [a, b] = router_pairs[i];
    auto [a_link, b_link] =
        nodes().LinkRemoteRouters(a, fragments[i], b, fragments[i]);
    a_link->MarkSideStable();
    b_link->MarkSideStable();
    links.emplace_back(std::move(a_link), std::move(b_link));
  }

  // We should be able to lock all links from either side, implying that both
  // sides have a valid reference to the same RouterLinkState.
  for (const auto& [a_link, b_link] : links) {
    EXPECT_TRUE(a_link->TryLockForClosure());
    a_link->Unlock();
    EXPECT_TRUE(b_link->TryLockForClosure());
  }

  CloseRoutes(router_pairs);
}

TEST_F(RemoteRouterLinkTest, NewLinkWithPendingState) {
  // Occupy all fragments in the primary buffer so they aren't usable.
  std::vector<FragmentRef<RouterLinkState>> unused_fragments =
      nodes().AllocateAllRouterLinkStates();

  // Now allocate another batch of fragments which must be in a newly allocated
  // buffer on node A. Because the nodes' transports are not active yet, there
  // is no way for node B to have had this buffer shared with it yet. Hence all
  // of these fragments will be seen as pending on node B.
  std::vector<FragmentRef<RouterLinkState>> fragments =
      nodes().AllocateAllRouterLinkStates();

  std::vector<Router::Pair> router_pairs =
      CreateTestRouterPairs(fragments.size());
  std::vector<RouterLink::Pair> links;
  for (size_t i = 0; i < fragments.size(); ++i) {
    auto [a, b] = router_pairs[i];
    auto a_state = fragments[i];
    auto b_fragment =
        nodes().memory_b().GetFragment(fragments[i].fragment().descriptor());
    auto b_state =
        nodes().memory_b().AdoptFragmentRef<RouterLinkState>(b_fragment);
    ASSERT_TRUE(a_state.is_addressable());
    ASSERT_TRUE(b_state.is_pending());
    auto [a_link, b_link] =
        nodes().LinkRemoteRouters(a, std::move(a_state), b, std::move(b_state));
    a_link->MarkSideStable();
    b_link->MarkSideStable();
    links.emplace_back(std::move(a_link), std::move(b_link));
  }

  // Because side B of these links still cannot resolve its RouterLinkState,
  // the link still cannot be stabilized or locked yet.
  for (const auto& [a_link, b_link] : links) {
    EXPECT_FALSE(a_link->TryLockForClosure());
    EXPECT_FALSE(b_link->TryLockForClosure());
  }

  // We're using the synchronous driver, so as soon as we activate our
  // transports, all pending NodeLink communications will complete before this
  // call returns. This also means side B of each link will resolve its
  // RouterLinkState.
  nodes().ActivateTransports();

  // Now all links should be lockable from either side, implying that both
  // sides have a valid reference to the same RouterLinkState.
  for (const auto& [a_link, b_link] : links) {
    EXPECT_TRUE(a_link->TryLockForClosure());
    a_link->Unlock();
    EXPECT_TRUE(b_link->TryLockForClosure());
  }

  CloseRoutes(router_pairs);
}

INSTANTIATE_TEST_SUITE_P(,
                         RouterLinkTest,
                         ::testing::Values(RouterLinkTestMode::kLocal,
                                           RouterLinkTestMode::kRemote));

}  // namespace
}  // namespace ipcz
