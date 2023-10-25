// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>
#include <string>
#include <string_view>
#include <thread>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

using MergePortalsTestNode = test::TestNode;
using MergePortalsTest = test::MultinodeTest<MergePortalsTestNode>;

constexpr std::string_view kMessage1 = "bork bork";
constexpr std::string_view kMessage2 = "aw heck";

MULTINODE_TEST_NODE(MergePortalsTestNode, MergeWithInitialPortalClient) {
  IpczHandle b = ConnectToBroker();

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message));
  EXPECT_EQ(kMessage1, message);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, kMessage2));
  WaitForDirectRemoteLink(b);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST(MergePortalsTest, MergeWithInitialPortal) {
  IpczHandle c = SpawnTestNode<MergeWithInitialPortalClient>();
  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Merge(c, p));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage1));

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message));
  EXPECT_EQ(kMessage2, message);
  WaitForDirectRemoteLink(q);
  Close(q);
}

MULTINODE_TEST(MergePortalsTest, MergeWithClosedLocalPeer) {
  auto [q, p] = OpenPortals();
  auto [d, b] = OpenPortals();

  Close(d);
  EXPECT_EQ(IPCZ_RESULT_OK, Merge(b, p));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(q, IPCZ_TRAP_PEER_CLOSED));
  Close(q);
}

MULTINODE_TEST_NODE(MergePortalsTestNode, MergeWithClosedRemotePeerClient) {
  IpczHandle b = ConnectToBroker();
  auto [q, p] = OpenPortals();
  Close(q);
  IpczHandle r;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&r, 1}));
  Merge(p, r);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST(MergePortalsTest, MergeWithClosedRemotePeer) {
  IpczHandle c = SpawnTestNode<MergeWithClosedRemotePeerClient>();
  auto [r, s] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "", {&r, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(s, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({c, s});
}

constexpr size_t kMergeComplexRoutesNumIterations = 1000;

MULTINODE_TEST_NODE(MergePortalsTestNode, MergeComplexRoutesClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle other_client;
  IpczHandle portal;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&other_client, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&portal, 1}));

  for (size_t i = 0; i < kMergeComplexRoutesNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, Put(other_client, "", {&portal, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(other_client, nullptr, {&portal, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&portal, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&portal, 1}));
  }

  WaitForDirectRemoteLink(portal);
  PingPong(portal);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "done"));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({b, portal, other_client});
}

MULTINODE_TEST(MergePortalsTest, MergeComplexRoutes) {
  IpczHandle c1 = SpawnTestNode<MergeComplexRoutesClient>();
  IpczHandle c2 = SpawnTestNode<MergeComplexRoutesClient>();

  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  auto [s, t] = OpenPortals();
  auto [u, v] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&t, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&v, 1}));

  for (size_t i = 0; i < kMergeComplexRoutesNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c1, nullptr, {&t, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&t, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c2, nullptr, {&v, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&v, 1}));
  }

  Merge(s, u);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c1, &message));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c2, &message));
  CloseAll({c1, c2});
}

MULTINODE_TEST_NODE(MergePortalsTestNode, RaceWithDisconnectClient) {
  IpczHandle b = ConnectToBroker();
  Put(b, "ping");

  // Small delay on this thread which gives some test drivers enough time
  // to wake up the broker node with the above ping and have them initiate
  // a portal merge. The race condition is extremely sensitive to timing, so
  // this still does not guarantee that the bug would be triggered if present.
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(300us);

  // Forcibly disconnect, simulating sudden client termination from the broker's
  // perspective.
  CloseThisNode();

  Close(b);
}

MULTINODE_TEST(MergePortalsTest, RaceWithDisconnect) {
  // Regression test for https://crbug.com/1495461. If the bug is present this
  // test becomes flaky.

  IpczHandle c = SpawnTestNode<RaceWithDisconnectClient>();

  // Wait for a message on `c`, indicating that handshake is done and there is
  // a direct connection between `c` and the client.
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_DEAD,
      .min_local_parcels = 0,
  };
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditions(c, conditions));

  // Initiate a merge with `c` and wait for `p` to observe peer closure, which
  // will be triggered by the client node's forced disconnection. Disconnection
  // propagation can race with the merge.
  auto [q, p] = OpenPortals();
  Merge(c, q);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(p, IPCZ_TRAP_PEER_CLOSED));
}

}  // namespace
}  // namespace ipcz
