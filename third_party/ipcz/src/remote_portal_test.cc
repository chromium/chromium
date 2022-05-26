// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

using RemotePortalTestNode = test::TestNode;
using RemotePortalTest = test::MultinodeTest<RemotePortalTestNode>;

static constexpr std::string_view kTestMessage1 = "hello world";
static constexpr std::string_view kTestMessage2 = "hola mundo";

MULTINODE_TEST_NODE(RemotePortalTestNode, BasicConnectionClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, kTestMessage1));

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message));
  EXPECT_EQ(kTestMessage2, message);
  Close(b);
}

TEST_P(RemotePortalTest, BasicConnection) {
  IpczHandle c = SpawnTestNode<BasicConnectionClient>();

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, &message));
  EXPECT_EQ(kTestMessage1, message);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kTestMessage2));
  Close(c);
}

MULTINODE_TEST_NODE(RemotePortalTestNode, PortalTransferClient) {
  IpczHandle b = ConnectToBroker();

  IpczHandle p = IPCZ_INVALID_HANDLE;
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&p, 1}));
  EXPECT_EQ(kTestMessage1, message);
  EXPECT_NE(IPCZ_INVALID_HANDLE, p);

  VerifyEndToEnd(p);
  CloseAll({p, b});
}

TEST_P(RemotePortalTest, PortalTransfer) {
  IpczHandle c = SpawnTestNode<PortalTransferClient>();

  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kTestMessage1, {&p, 1}));

  VerifyEndToEnd(q);
  CloseAll({q, c});
}

constexpr size_t kMultipleHopsNumIterations = 100;

MULTINODE_TEST_NODE(RemotePortalTestNode, MultipleHopsClient1) {
  IpczHandle b = ConnectToBroker();

  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&p, 1}));

  for (size_t i = 0; i < kMultipleHopsNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kTestMessage2));
  }

  for (size_t i = 0; i < kMultipleHopsNumIterations; ++i) {
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message));
    EXPECT_EQ(kTestMessage1, message);
  }

  CloseAll({q, b});
}

MULTINODE_TEST_NODE(RemotePortalTestNode, MultipleHopsClient2) {
  IpczHandle b = ConnectToBroker();

  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));

  for (size_t i = 0; i < kMultipleHopsNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, Put(p, kTestMessage1));
  }

  for (size_t i = 0; i < kMultipleHopsNumIterations; ++i) {
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(p, &message));
    EXPECT_EQ(kTestMessage2, message);
  }

  CloseAll({p, b});
}

TEST_P(RemotePortalTest, MultipleHops) {
  IpczHandle c1 = SpawnTestNode<MultipleHopsClient1>();
  IpczHandle c2 = SpawnTestNode<MultipleHopsClient2>();

  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c1, nullptr, {&p, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  CloseAll({c1, c2});
}

constexpr size_t kTransferBackAndForthNumIterations = 1;

MULTINODE_TEST_NODE(RemotePortalTestNode, TransferBackAndForthClient) {
  IpczHandle b = ConnectToBroker();

  for (size_t i = 0; i < kTransferBackAndForthNumIterations; ++i) {
    IpczHandle p;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&p, 1}));
  }

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

TEST_P(RemotePortalTest, TransferBackAndForth) {
  IpczHandle c = SpawnTestNode<TransferBackAndForthClient>();

  constexpr std::string_view kMessage = "hihihihi";
  auto [q, p] = OpenPortals();
  std::string message;
  for (size_t i = 0; i < kTransferBackAndForthNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "", {&p, 1}));
    p = IPCZ_INVALID_HANDLE;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, nullptr, {&p, 1}));
    EXPECT_NE(IPCZ_INVALID_HANDLE, p);
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(p, &message));
    EXPECT_EQ(kMessage, message);
  }

  VerifyEndToEndLocal(q, p);
  CloseAll({q, p, c});
}

MULTINODE_TEST_NODE(RemotePortalTestNode, DisconnectThroughProxyClient1) {
  IpczHandle b = ConnectToBroker();

  IpczHandle q;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));

  // Should eventually be observed by virtue of the forced disconnection of
  // client 3 in the main test body.
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(q, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({q, b});
}

MULTINODE_TEST_NODE(RemotePortalTestNode, DisconnectThroughProxyClient2) {
  IpczHandle b = ConnectToBroker();

  // Receive a portal p from the broker and bounce it back.
  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&p, 1}));

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST_NODE(RemotePortalTestNode, DisconnectThroughProxyClient3) {
  IpczHandle b = ConnectToBroker();

  // Receive a portal p from the broker and then immediately terminate this
  // node.
  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));

  // Forcibly shut this node down, severing all connections to other nodes. We
  // do this *before* closing `p` to ensure that we are't just exercising normal
  // portal closure, which other tests already cover. From the perspective of
  // other nodes, we are effectively simulating a crash of this node.
  CloseThisNode();

  // It's still necessary to explicitly close local portals after the node
  // itself has been shut down. Otherwise they'd leak.
  CloseAll({p, b});
}

TEST_P(RemotePortalTest, DisconnectThroughProxy) {
  // Exercises node disconnection. Namely if portals on nodes 1 and 3 are
  // connected via proxy on node 2, and node 3 disappears, node 1's portal
  // should observe peer closure.
  IpczHandle c1, c3;
  auto c1_control = SpawnTestNode<DisconnectThroughProxyClient1>({&c1, 1});
  IpczHandle c2 = SpawnTestNode<DisconnectThroughProxyClient2>();
  auto c3_control = SpawnTestNode<DisconnectThroughProxyClient3>({&c3, 1});

  auto [q, p] = OpenPortals();

  // We send q to client 1, and p to client 2.
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  // Client 2 forwards p back to us, and we forward it now to client 3. This
  // process ensures that client 2 will for some time serve as a proxy between
  // client 1 and client 3.
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c2, nullptr, {&p, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c3, "", {&p, 1}));

  // Client 3 will terminate on its own. Though not determinstic, this will
  // race with proxy reduction such that client 2 may still be proxying for the
  // q-p portal pair when client 3 (who owns p) goes away.
  EXPECT_TRUE(c3_control->WaitForShutdown());

  // Client 1 waits on q observing peer closure before terminating. This will
  // block until that happens.
  EXPECT_TRUE(c1_control->WaitForShutdown());
  CloseAll({c1, c2, c3});
}

INSTANTIATE_MULTINODE_TEST_SUITE_P(RemotePortalTest);

}  // namespace
}  // namespace ipcz
