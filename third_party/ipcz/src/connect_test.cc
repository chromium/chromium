// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipcz/ipcz.h"
#include "ipcz/node_messages.h"
#include "reference_drivers/blob.h"
#include "test/multinode_test.h"
#include "test/test_transport_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ipcz {
namespace {

using ConnectTestNode = test::TestNode;
using ConnectTest = test::MultinodeTest<ConnectTestNode>;

MULTINODE_TEST_NODE(ConnectTestNode, BrokerToNonBrokerClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

TEST_P(ConnectTest, BrokerToNonBroker) {
  IpczHandle c = SpawnTestNode<BrokerToNonBrokerClient>();
  Close(c);
}

constexpr size_t kNumBrokerPortals = 2;
constexpr size_t kNumNonBrokerPortals = 5;
static_assert(kNumBrokerPortals < kNumNonBrokerPortals,
              "Test requires fewer broker portals than non-broker portals");

MULTINODE_TEST_NODE(ConnectTestNode, SurplusPortalsClient) {
  IpczHandle portals[kNumNonBrokerPortals];
  ConnectToBroker(portals);

  // All of the surplus portals should observe peer closure.
  for (size_t i = kNumBrokerPortals; i < kNumNonBrokerPortals; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK,
              WaitForConditionFlags(portals[i], IPCZ_TRAP_PEER_CLOSED));
  }
  CloseAll(portals);
}

TEST_P(ConnectTest, SurplusPortals) {
  IpczHandle portals[kNumBrokerPortals];
  SpawnTestNode<SurplusPortalsClient>(portals);
  CloseAll(portals);
}

MULTINODE_TEST_NODE(ConnectTestNode, ExpectDisconnectFromBroker) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

TEST_P(ConnectTest, DisconnectWithoutBrokerHandshake) {
  TransportPair transports = CreateTransports();
  auto controller =
      SpawnTestNodeWithTransport<ExpectDisconnectFromBroker>(transports.theirs);
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Close(transports.ours, IPCZ_NO_FLAGS, nullptr));
  controller->WaitForShutdown();
}

MULTINODE_TEST_NODE(ConnectTestNode,
                    DisconnectWithoutNonBrokerHandshakeClient) {
  // Our transport is automatically closed on exit. No handshake is sent because
  // we never call ConnectToBroker(). No action required.
}

TEST_P(ConnectTest, DisconnectWithoutNonBrokerHandshake) {
  IpczHandle c = SpawnTestNode<DisconnectWithoutNonBrokerHandshakeClient>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

TEST_P(ConnectTest, DisconnectOnBadBrokerMessage) {
  TransportPair transports = CreateTransports();
  auto controller =
      SpawnTestNodeWithTransport<ExpectDisconnectFromBroker>(transports.theirs);

  // Send some garbage to the other node.
  const char kBadMessage[] = "this will never be a valid handshake message!";
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      GetDriver().Transmit(transports.ours, kBadMessage, std::size(kBadMessage),
                           nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Close(transports.ours, IPCZ_NO_FLAGS, nullptr));

  // The other node will only shut down once it's observed peer closure on its
  // portal to us; which it should, because we just sent it some garbage.
  controller->WaitForShutdown();
}

MULTINODE_TEST_NODE(ConnectTestNode, TransmitSomeGarbage) {
  // Instead of doing the usual connection dance, send some garbage back to the
  // broker. It should disconnect ASAP.
  const char kBadMessage[] = "this will never be a valid handshake message!";
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      GetDriver().Transmit(transport(), kBadMessage, std::size(kBadMessage),
                           nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  test::TestTransportListener listener(node(), ReleaseTransport());
  absl::Notification done;
  listener.OnError([&done] { done.Notify(); });
  done.WaitForNotification();
  listener.StopListening();
}

TEST_P(ConnectTest, DisconnectOnBadNonBrokerMessage) {
  IpczHandle c;
  auto controller = SpawnTestNode<TransmitSomeGarbage>({&c, 1});

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);

  // Make sure the client also observes disconnection of its transport. It won't
  // terminate until that happens.
  controller->WaitForShutdown();
}

constexpr std::string_view kBlob1Contents = "from q";
constexpr std::string_view kBlob2Contents = "from p";

MULTINODE_TEST_NODE(ConnectTestNode, NonBrokerToNonBrokerClientChild) {
  IpczHandle parent = ConnectToParent(IPCZ_CONNECT_NODE_INHERIT_BROKER);

  std::string expected_contents;
  IpczHandle portal;
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitToGet(parent, &expected_contents, {&portal, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, nullptr, {&box, 1}));
  EXPECT_EQ(expected_contents, UnboxBlob(box));

  PingPong(portal);
  WaitForDirectRemoteLink(portal);
  CloseAll({parent, portal});
}

MULTINODE_TEST_NODE(ConnectTestNode, NonBrokerToNonBrokerClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle c = SpawnTestNode<NonBrokerToNonBrokerClientChild>(
      IPCZ_CONNECT_NODE_SHARE_BROKER);

  std::string expected_contents;
  IpczHandle portal;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &expected_contents, {&portal, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, expected_contents, {&portal, 1}));

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({c, b});
}

TEST_P(ConnectTest, NonBrokerToNonBroker) {
  IpczHandle c1 = SpawnTestNode<NonBrokerToNonBrokerClient>();
  IpczHandle c2 = SpawnTestNode<NonBrokerToNonBrokerClient>();

  auto [q, p] = OpenPortals();
  IpczHandle q_box = BoxBlob(kBlob1Contents);
  IpczHandle p_box = BoxBlob(kBlob2Contents);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(q, "", {&q_box, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(p, "", {&p_box, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, kBlob2Contents, {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, kBlob1Contents, {&p, 1}));

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c1, IPCZ_TRAP_PEER_CLOSED));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c2, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({c1, c2});
}

MULTINODE_TEST_NODE(ConnectTestNode, BadNonBrokerReferralClient) {
  IpczHandle b = ConnectToBroker();

  TransportPair transports = CreateTransports();

  // Transmit something invalid from the referred node's side of the transport.
  const char kBadMessage[] = "i am a terrible node plz reject";
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Transmit(transports.theirs, kBadMessage,
                                 std::size(kBadMessage), nullptr, 0,
                                 IPCZ_NO_FLAGS, nullptr));

  auto ignore_activity =
      [](IpczHandle, const void*, size_t, const IpczDriverHandle*, size_t,
         IpczTransportActivityFlags, const void*) { return IPCZ_RESULT_OK; };
  EXPECT_EQ(IPCZ_RESULT_OK, GetDriver().ActivateTransport(
                                transports.theirs, IPCZ_INVALID_HANDLE,
                                ignore_activity, IPCZ_NO_FLAGS, nullptr));

  // Now refer our imaginary other node using our end of the transport. The
  // broker should reject the referral and we should eventually observe
  // disconnection of our initial portal to the referred node.
  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().ConnectNode(node(), transports.ours, 1,
                               IPCZ_CONNECT_NODE_SHARE_BROKER, nullptr, &p));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(p, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({b, p});

  EXPECT_EQ(IPCZ_RESULT_OK, GetDriver().DeactivateTransport(
                                transports.theirs, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Close(transports.theirs, IPCZ_NO_FLAGS, nullptr));
}

TEST_P(ConnectTest, BadNonBrokerReferral) {
  IpczHandle c = SpawnTestNode<BadNonBrokerReferralClient>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

MULTINODE_TEST_NODE(ConnectTestNode, FailedNonBrokerReferralReferredClient) {
  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().ConnectNode(node(), ReleaseTransport(), 1,
                               IPCZ_CONNECT_NODE_INHERIT_BROKER, nullptr, &p));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(p, IPCZ_TRAP_PEER_CLOSED));
  Close(p);
}

MULTINODE_TEST_NODE(ConnectTestNode, FailedNonBrokerReferralClient) {
  IpczHandle b = ConnectToBroker();

  TransportPair transports = CreateTransports();
  auto controller =
      SpawnTestNodeWithTransport<FailedNonBrokerReferralReferredClient>(
          transports.theirs);

  // Disconnect the transport instead of passing to our broker with
  // ConnectNode(). The referred client should observe disconnection of its
  // initial portals and terminate itself.
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().Close(transports.ours, IPCZ_NO_FLAGS, nullptr));
  controller->WaitForShutdown();
  Close(b);
}

TEST_P(ConnectTest, FailedNonBrokerReferral) {
  IpczHandle c = SpawnTestNode<FailedNonBrokerReferralClient>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

INSTANTIATE_MULTINODE_TEST_SUITE_P(ConnectTest);

}  // namespace
}  // namespace ipcz
