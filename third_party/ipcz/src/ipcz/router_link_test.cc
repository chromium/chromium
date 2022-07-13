// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router_link.h"

#include <tuple>

#include "ipcz/driver_transport.h"
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

class RouterLinkTest : public testing::Test,
                       public testing::WithParamInterface<RouterLinkTestMode> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case RouterLinkTestMode::kLocal:
        std::tie(a_link_, b_link_) =
            LocalRouterLink::ConnectRouters(LinkType::kCentral, {a_, b_});
        break;

      case RouterLinkTestMode::kRemote: {
        auto transports = DriverTransport::CreatePair(kTestDriver);
        auto alloc = NodeLinkMemory::Allocate(broker_);
        broker_node_link_ = NodeLink::Create(
            broker_, LinkSide::kA, kTestBrokerName, kTestNonBrokerName,
            Node::Type::kNormal, 0, transports.first,
            std::move(alloc.node_link_memory));
        non_broker_node_link_ = NodeLink::Create(
            non_broker_, LinkSide::kB, kTestNonBrokerName, kTestBrokerName,
            Node::Type::kBroker, 0, transports.second,
            NodeLinkMemory::Adopt(non_broker_,
                                  std::move(alloc.primary_buffer_memory)));
        broker_->AddLink(kTestNonBrokerName, broker_node_link_);
        non_broker_->AddLink(kTestBrokerName, non_broker_node_link_);

        auto fragment = broker_node_link_->memory().AllocateFragment(
            sizeof(RouterLinkState));
        auto link_state = FragmentRef<RouterLinkState>(
            RefCountedFragment::kAdoptExistingRef,
            WrapRefCounted(&broker_node_link_->memory()), fragment);
        RouterLinkState::Initialize(link_state.get());
        a_link_ = broker_node_link_->AddRemoteRouterLink(
            SublinkId{0}, link_state, LinkType::kCentral, LinkSide::kA, a_);
        b_link_ = non_broker_node_link_->AddRemoteRouterLink(
            SublinkId{0}, link_state, LinkType::kCentral, LinkSide::kB, b_);
        a_->SetOutwardLink(a_link_);
        b_->SetOutwardLink(b_link_);

        broker_node_link_->transport()->Activate();
        non_broker_node_link_->transport()->Activate();
        break;
      }
    }

    ASSERT_EQ(a_link_->GetLinkState(), b_link_->GetLinkState());
    link_state_ = a_link_->GetLinkState();
  }

  void TearDown() override {
    a_->CloseRoute();
    b_->CloseRoute();
    broker_->Close();
    non_broker_->Close();
  }

  Router& a() { return *a_; }
  Router& b() { return *b_; }
  RouterLink& a_link() { return *a_link_; }
  RouterLink& b_link() { return *b_link_; }
  RouterLinkState& link_state() { return *link_state_; }
  RouterLinkState::Status link_status() { return link_state_->status; }

 private:
  const Ref<Node> broker_{MakeRefCounted<Node>(Node::Type::kBroker,
                                               kTestDriver,
                                               IPCZ_INVALID_DRIVER_HANDLE)};
  const Ref<Node> non_broker_{MakeRefCounted<Node>(Node::Type::kNormal,
                                                   kTestDriver,
                                                   IPCZ_INVALID_DRIVER_HANDLE)};
  Ref<NodeLink> broker_node_link_;
  Ref<NodeLink> non_broker_node_link_;

  const Ref<Router> a_{MakeRefCounted<Router>()};
  const Ref<Router> b_{MakeRefCounted<Router>()};
  Ref<RouterLink> a_link_;
  Ref<RouterLink> b_link_;
  RouterLinkState* link_state_ = nullptr;
};

TEST_P(RouterLinkTest, Locking) {
  EXPECT_EQ(RouterLinkState::kUnstable, link_status());

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

INSTANTIATE_TEST_SUITE_P(,
                         RouterLinkTest,
                         ::testing::Values(RouterLinkTestMode::kLocal,
                                           RouterLinkTestMode::kRemote));

}  // namespace
}  // namespace ipcz
