// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "ipcz/ipcz.h"
#include "ipcz/node_messages.h"
#include "reference_drivers/async_reference_driver.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test/multinode_test.h"
#include "test/test_transport_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ipcz {
namespace {

class ConnectTestNode : public test::TestNode {
 public:
  void ActivateAndClose(IpczDriverHandle transport) {
    // Registering any listener callback activates the transport, and
    // listener destruction closes it.
    test::TestTransportListener listener(node(), transport);
    listener.OnError([] {});
  }
};

using ConnectTest = test::MultinodeTest<ConnectTestNode>;

MULTINODE_TEST_NODE(ConnectTestNode, BrokerToNonBrokerClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST(ConnectTest, BrokerToNonBroker) {
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

MULTINODE_TEST(ConnectTest, SurplusPortals) {
  IpczHandle portals[kNumBrokerPortals];
  SpawnTestNode<SurplusPortalsClient>(portals);
  CloseAll(portals);
}

MULTINODE_TEST_NODE(ConnectTestNode, ExpectDisconnectFromBroker) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST(ConnectTest, DISABLED_DisconnectWithoutBrokerHandshake) {
  IpczDriverHandle our_transport;
  auto controller =
      SpawnTestNodeNoConnect<ExpectDisconnectFromBroker>(our_transport);
  ActivateAndClose(our_transport);
  controller->WaitForShutdown();
}

MULTINODE_TEST_NODE(ConnectTestNode,
                    DisconnectWithoutNonBrokerHandshakeClient) {
  // Our transport is automatically closed on exit. No handshake is sent because
  // we never call ConnectToBroker(). No action required.
}

MULTINODE_TEST(ConnectTest, DisconnectWithoutNonBrokerHandshake) {
  IpczHandle c = SpawnTestNode<DisconnectWithoutNonBrokerHandshakeClient>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

MULTINODE_TEST(ConnectTest, DisconnectOnBadBrokerMessage) {
  IpczDriverHandle our_transport;
  auto controller =
      SpawnTestNodeNoConnect<ExpectDisconnectFromBroker>(our_transport);

  // Send some garbage to the other node.
  const char kBadMessage[] = "this will never be a valid handshake message!";
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      GetDriver().Transmit(our_transport, kBadMessage, std::size(kBadMessage),
                           nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  ActivateAndClose(our_transport);

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

MULTINODE_TEST(ConnectTest, DisconnectOnBadNonBrokerMessage) {
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

MULTINODE_TEST(ConnectTest, NonBrokerToNonBroker) {
#if BUILDFLAG(IS_ANDROID)
  // Client nodes launching other client nodes doesn't work for Chromium's
  // custom test driver on Android. Limit this test to the reference test
  // drivers there.
  if (&GetDriver() != &reference_drivers::kSyncReferenceDriver &&
      &GetDriver() != &reference_drivers::kAsyncReferenceDriver) {
    return;
  }
#endif

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

MULTINODE_TEST(ConnectTest, BadNonBrokerReferral) {
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

  IpczDriverHandle our_transport;
  auto controller =
      SpawnTestNodeNoConnect<FailedNonBrokerReferralReferredClient>(
          our_transport);

  // Activate and immediately disconnect the transport instead of passing to our
  // broker with ConnectNode(). The referred client should observe disconnection
  // of its initial portals and terminate itself.
  ActivateAndClose(our_transport);
  controller->WaitForShutdown();
  Close(b);
}

MULTINODE_TEST(ConnectTest, FailedNonBrokerReferral) {
  IpczHandle c = SpawnTestNode<FailedNonBrokerReferralClient>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

MULTINODE_TEST_BROKER_NODE(ConnectTestNode, AnotherBroker) {
  IpczHandle b = ConnectToBroker();
  PingPong(b);
  Close(b);
}

MULTINODE_TEST(ConnectTest, BrokerToBroker) {
  IpczHandle b = SpawnTestNode<AnotherBroker>();

  PingPong(b);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST_NODE(ConnectTestNode, BrokerClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle other_client;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&other_client, 1}));

  // Ensure that we end up with a direct connection to the other client, which
  // implies the two non-broker nodes have been properly introduced across the
  // boundary of their respective node networks.
  PingPong(other_client);
  WaitForDirectRemoteLink(other_client);

  // Synchronize against the main test node. See synchronization comment there.
  PingPong(b);
  CloseAll({b, other_client});
}

MULTINODE_TEST_BROKER_NODE(ConnectTestNode, BrokerWithClientNode) {
  IpczHandle b = ConnectToBroker();
  IpczHandle client = SpawnTestNode<BrokerClient>();

  IpczHandle other_client;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&other_client, 1}));
  Put(client, "", {&other_client, 1});

  // Synchronize against the launched client to ensure that it's done before we
  // join it and terminate.
  PingPong(client);
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(client, IPCZ_TRAP_PEER_CLOSED));

  // Synchronize against the main test node. See synchronization comment there.
  PingPong(b);
  CloseAll({b, client});
}

MULTINODE_TEST(ConnectTest, MultiBrokerIntroductions) {
  // This test covers introductions in a multi-broker network. There are four
  // test nodes involved here: the main node (this one, call it A), a secondary
  // broker B launched with the BrokerWithClientNode body defined above; and
  // two client nodes (running BrokerClient above) we will call C and D, with
  // C launched by A and D launched by B.
  //
  // A portal pair is created on A and its portals are passed to node B (our
  // secondary broker) and node C (the singular non-broker client node in A's
  // local network) respectively.
  //
  // Node B in turn passes its end to its own launched non-broker client D. This
  // ultimately elicits a need for node C to be introduced to node D. The test
  // succeeds only once the portal on node C appears to be directly connected to
  // the portal on node D -- and vice versa -- implying successful introduction.

  IpczHandle other_broker = SpawnTestNode<BrokerWithClientNode>();
  IpczHandle client = SpawnTestNode<BrokerClient>();

  auto [q, p] = OpenPortals();
  Put(other_broker, "", {&q, 1});
  Put(client, "", {&p, 1});

  // Synchronize against both the launched broker and the launched client node
  // to ensure that they're done before we join them and terminate.
  PingPong(other_broker);
  PingPong(client);
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(other_broker, IPCZ_TRAP_PEER_CLOSED));
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditionFlags(client, IPCZ_TRAP_PEER_CLOSED));

  CloseAll({other_broker, client});
}

}  // namespace
}  // namespace ipcz
