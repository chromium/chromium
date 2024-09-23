// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node.h"

#include <utility>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_transport.h"
#include "ipcz/features.h"
#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_name.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

constexpr NodeName kNodeAName(0, 1);
constexpr NodeName kNodeBName(1, 2);
constexpr NodeName kNodeCName(3, 5);

class NodeTest : public testing::Test {
 public:
  void SetUp() override {
    ConnectBrokerToNode(node_a_, kNodeAName);
    ConnectBrokerToNode(node_b_, kNodeBName);
  }

  void TearDown() override {
    node_b_->Close();
    node_a_->Close();
    broker_->Close();
  }

  Node& broker() { return *broker_; }
  NodeName broker_name() { return broker_->GetAssignedName(); }
  Node& node_a() { return *node_a_; }
  Node& node_b() { return *node_b_; }

 private:
  void ConnectBrokerToNode(Ref<Node> node, const NodeName& name) {
    auto transports = DriverTransport::CreatePair(kTestDriver);
    DriverMemoryWithMapping buffer =
        NodeLinkMemory::AllocateMemory(kTestDriver);
    const NodeName broker_name = broker_->GetAssignedName();
    auto broker_link = NodeLink::CreateInactive(
        broker_, LinkSide::kA, broker_name, name, Node::Type::kNormal, 0,
        Features{}, transports.first,
        NodeLinkMemory::Create(broker_, LinkSide::kA, Features{},
                               std::move(buffer.mapping)));
    auto node_link = NodeLink::CreateInactive(
        node, LinkSide::kB, name, broker_name, Node::Type::kBroker, 0,
        Features{}, transports.second,
        NodeLinkMemory::Create(node, LinkSide::kB, Features{},
                               buffer.memory.Map()));
    broker_->AddConnection(name, {.link = broker_link});
    node->AddConnection(broker_name, {.link = node_link, .broker = node_link});
    broker_link->Activate();
    node_link->Activate();
  }

  const Ref<Node> broker_{
      MakeRefCounted<Node>(Node::Type::kBroker, kTestDriver)};
  const Ref<Node> node_a_{
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver)};
  const Ref<Node> node_b_{
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver)};
};

TEST_F(NodeTest, EstablishExistingLinks) {
  // When the requested node is already known, EstablishLink() responds
  // immediately with the existing NodeLink.

  bool called = false;
  auto expect_link = [&called](Ref<NodeLink>& expected) {
    return [expected = expected.get(), &called](NodeLink* link) {
      EXPECT_EQ(expected, link);
      called = true;
    };
  };

  Ref<NodeLink> broker_to_a = broker().GetLink(kNodeAName);
  EXPECT_TRUE(broker_to_a);
  broker().EstablishLink(kNodeAName, expect_link(broker_to_a));
  EXPECT_TRUE(called);
  called = false;

  Ref<NodeLink> broker_to_b = broker().GetLink(kNodeBName);
  EXPECT_TRUE(broker_to_b);
  broker().EstablishLink(kNodeBName, expect_link(broker_to_b));
  EXPECT_TRUE(called);
  called = false;

  Ref<NodeLink> a_to_broker = node_a().GetLink(broker_name());
  EXPECT_TRUE(a_to_broker);
  node_a().EstablishLink(broker_name(), expect_link(a_to_broker));
  EXPECT_TRUE(called);
  called = false;

  Ref<NodeLink> b_to_broker = node_b().GetLink(broker_name());
  EXPECT_TRUE(b_to_broker);
  node_b().EstablishLink(broker_name(), expect_link(b_to_broker));
  EXPECT_TRUE(called);
  called = false;
}

TEST_F(NodeTest, EstablishNewLinks) {
  // When the requested node is not yet known to the caller but is known to
  // their broker, EstablishLink() coordinates with the broker to get the
  // two nodes introduced to each other.

  NodeLink* established_link = nullptr;
  EXPECT_FALSE(node_a().GetLink(kNodeBName));
  EXPECT_FALSE(node_b().GetLink(kNodeAName));
  node_a().EstablishLink(kNodeBName, [&established_link](NodeLink* link) {
    established_link = link;
  });
  EXPECT_TRUE(established_link);

  Ref<NodeLink> a_to_b = node_a().GetLink(kNodeBName);
  Ref<NodeLink> b_to_a = node_b().GetLink(kNodeAName);
  EXPECT_TRUE(a_to_b);
  EXPECT_TRUE(b_to_a);
  EXPECT_EQ(a_to_b.get(), established_link);

  // A redundant EstablishLink() changes nothing, even from the other side.
  node_a().EstablishLink(
      kNodeBName, [&a_to_b](NodeLink* link) { EXPECT_EQ(a_to_b.get(), link); });
  node_b().EstablishLink(
      kNodeAName, [&b_to_a](NodeLink* link) { EXPECT_EQ(b_to_a.get(), link); });
  EXPECT_EQ(a_to_b, node_a().GetLink(kNodeBName));
  EXPECT_EQ(b_to_a, node_b().GetLink(kNodeAName));

  // Verify that the new links are managing a common buffer pool.
  constexpr uint64_t kMagic = 0x1123581321345589;
  const Fragment a_fragment = a_to_b->memory().AllocateFragment(8);
  EXPECT_TRUE(a_fragment.is_addressable());
  *static_cast<uint64_t*>(a_fragment.address()) = kMagic;

  const Fragment b_fragment =
      b_to_a->memory().GetFragment(a_fragment.descriptor());
  EXPECT_TRUE(b_fragment.is_addressable());
  EXPECT_EQ(kMagic, *static_cast<uint64_t*>(b_fragment.address()));
  *static_cast<uint64_t*>(b_fragment.address()) = 0;
  EXPECT_EQ(0u, *static_cast<uint64_t*>(a_fragment.address()));
}

TEST_F(NodeTest, EstablishLinkFailureFromNonBroker) {
  // If the named node is unknown to the broker, a link can't be established by
  // a non-broker.
  bool failed = false;
  EXPECT_FALSE(broker().GetLink(kNodeCName));
  EXPECT_FALSE(node_a().GetLink(kNodeCName));
  node_a().EstablishLink(kNodeCName, [&](NodeLink* link) {
    EXPECT_FALSE(link);
    failed = true;
  });
  EXPECT_TRUE(failed);
}

TEST_F(NodeTest, EstablishLinkFailureFromBroker) {
  // New links can't be automatically established by the broker.
  bool failed = false;
  EXPECT_FALSE(broker().GetLink(kNodeCName));
  broker().EstablishLink(kNodeCName, [&](NodeLink* link) {
    EXPECT_FALSE(link);
    failed = true;
  });
  EXPECT_TRUE(failed);
}

TEST_F(NodeTest, EstablishLinkFailureWithoutBrokerLink) {
  // A node with no broker link can't be introduced to anyone.
  bool failed = false;
  const Ref<Node> node_c =
      MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver);
  EXPECT_TRUE(broker().GetLink(kNodeAName));
  node_c->EstablishLink(kNodeAName, [&](NodeLink* link) {
    EXPECT_FALSE(link);
    failed = true;
  });
  EXPECT_TRUE(failed);
}

}  // namespace
}  // namespace ipcz
