// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipcz/ipcz.h"
#include "ipcz/node_messages.h"
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
      SpawnTestNode<ExpectDisconnectFromBroker>(transports.theirs);
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
      SpawnTestNode<ExpectDisconnectFromBroker>(transports.theirs);

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

INSTANTIATE_MULTINODE_TEST_SUITE_P(ConnectTest);

}  // namespace
}  // namespace ipcz
