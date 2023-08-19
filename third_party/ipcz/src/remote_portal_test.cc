// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>

#include "build/build_config.h"
#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"

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

MULTINODE_TEST(RemotePortalTest, BasicConnection) {
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
  WaitForDirectRemoteLink(p);
  CloseAll({p, b});
}

MULTINODE_TEST(RemotePortalTest, PortalTransfer) {
  IpczHandle c = SpawnTestNode<PortalTransferClient>();

  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kTestMessage1, {&p, 1}));

  VerifyEndToEnd(q);
  WaitForDirectRemoteLink(q);
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

  WaitForDirectRemoteLink(q);
  PingPong(b);
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

  WaitForDirectRemoteLink(p);
  PingPong(b);
  CloseAll({p, b});
}

MULTINODE_TEST(RemotePortalTest, MultipleHops) {
  IpczHandle c1 = SpawnTestNode<MultipleHopsClient1>();
  IpczHandle c2 = SpawnTestNode<MultipleHopsClient2>();

  IpczHandle p;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c1, nullptr, {&p, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  WaitForPingAndReply(c1);
  WaitForPingAndReply(c2);
  CloseAll({c1, c2});
}

constexpr size_t kTransferBackAndForthNumIterations = 100;

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

MULTINODE_TEST(RemotePortalTest, TransferBackAndForth) {
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
  WaitForDirectLocalLink(q, p);
  CloseAll({q, p, c});
}

constexpr size_t kHugeNumberOfPortalsCount = 1000;

MULTINODE_TEST_NODE(RemotePortalTestNode, HugeNumberOfPortalsClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle other_client;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&other_client, 1}));

  std::vector<IpczHandle> portals(kHugeNumberOfPortalsCount);
  for (auto& portal : portals) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&portal, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(other_client, "", {&portal, 1}));
  }

  for (size_t i = 0; i < kHugeNumberOfPortalsCount; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK,
              WaitToGet(other_client, nullptr, {&portals[i], 1}));

    IpczHandle box = BoxBlob(absl::StrCat(absl::Dec(i)));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(portals[i], "", {&box, 1}));

    box = IPCZ_INVALID_HANDLE;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portals[i], nullptr, {&box, 1}));
    EXPECT_EQ(absl::StrCat(absl::Dec(i)), UnboxBlob(box));
  }

  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&other_client, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  CloseAll(portals);
  Close(b);
}

MULTINODE_TEST(RemotePortalTest, HugeNumberOfPortals) {
  // Opens a very large number of portals, and sends them all to client nodes.
  // The client nodes exchange these portals with each other and transmit
  // parcels over them, with and without driver objects. This exercises
  // NodeLinkMemory allocations and related synchronization around e.g.
  // delegated allocation and driver object relaying.

  IpczHandle c1 = SpawnTestNode<HugeNumberOfPortalsClient>();
  IpczHandle c2 = SpawnTestNode<HugeNumberOfPortalsClient>();
  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));
  for (size_t i = 0; i < kHugeNumberOfPortalsCount; ++i) {
    auto [a, b] = OpenPortals();
    EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&a, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&b, 1}));
  }

  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c1, nullptr, {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c2, nullptr, {&p, 1}));
  WaitForDirectLocalLink(q, p);
  CloseAll({c1, c2, q, p});
}

constexpr size_t kRouteExpansionStressTestNumIterations = 100;

MULTINODE_TEST_NODE(RemotePortalTestNode, RoutingStressTestClient) {
  // Each client only expects portals from the broker and bounces them back.
  IpczHandle b = ConnectToBroker();
  for (size_t i = 0; i < kRouteExpansionStressTestNumIterations; ++i) {
    IpczHandle p;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "", {&p, 1}));
  }
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

MULTINODE_TEST(RemotePortalTest, RoutingStressTest) {
  // This test spawns a bunch of nodes and bounces two portals back and forth
  // among them over a large number of iterations, then waits for all the
  // intermediate routers to be removed. Every iteration also sends a message
  // from each end of the pipe to exercise parcel routing, including with
  // driver objects attached.

  constexpr size_t kNumClientPairs = 3;
  std::pair<IpczHandle, IpczHandle> client_pairs[kNumClientPairs];
  for (auto& pair : client_pairs) {
    pair = std::make_pair(SpawnTestNode<RoutingStressTestClient>(),
                          SpawnTestNode<RoutingStressTestClient>());
  }

  auto [a, b] = OpenPortals();
  for (size_t j = 0; j < kRouteExpansionStressTestNumIterations; ++j) {
    IpczHandle box = BoxBlob(absl::StrCat(absl::Dec(j)));
    EXPECT_EQ(IPCZ_RESULT_OK,
              Put(a, absl::StrCat("a", absl::Dec(j)), {&box, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(b, absl::StrCat("b", absl::Dec(j))));
    for (auto& pair : client_pairs) {
      EXPECT_EQ(IPCZ_RESULT_OK, Put(pair.first, "", {&a, 1}));
      EXPECT_EQ(IPCZ_RESULT_OK, Put(pair.second, "", {&b, 1}));
      EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(pair.first, nullptr, {&a, 1}));
      EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(pair.second, nullptr, {&b, 1}));
    }
  }

  WaitForDirectLocalLink(a, b);

  for (size_t i = 0; i < kRouteExpansionStressTestNumIterations; ++i) {
    std::string message;
    IpczHandle box;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(a, &message));
    EXPECT_EQ(absl::StrCat("b", absl::Dec(i)), message);
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
    EXPECT_EQ(absl::StrCat("a", absl::Dec(i)), message);
    EXPECT_EQ(absl::StrCat(absl::Dec(i)), UnboxBlob(box));
  }

  for (auto& pair : client_pairs) {
    CloseAll({pair.first, pair.second});
  }
  CloseAll({a, b});
}

// For adequate test coverage this number needs to be large enough to trigger
// allocation of at least one new 64-byte BlockAllocator by the
// ClosurePropagationClient1 test node, which will occur approximately once for
// every 1024 routes created. We create a good bit more than that to be sure.
constexpr size_t kNumPortalsForClosurePropagation = 3000;

MULTINODE_TEST_NODE(RemotePortalTestNode, ClosurePropagationClient1) {
  IpczHandle b = ConnectToBroker();
  WaitForDirectRemoteLink(b);

  IpczHandle wait_for_c2;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&wait_for_c2, 1}));
  WaitForDirectRemoteLink(wait_for_c2);
  Put(b, "ok");

  IpczHandle qs[kNumPortalsForClosurePropagation];
  for (IpczHandle& q : qs) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));
  }

  for (IpczHandle& q : qs) {
    WaitForConditionFlags(q, IPCZ_TRAP_PEER_CLOSED);
  }

  Put(b, "done");
  EXPECT_EQ("bye", WaitToGetString(b));
  Close(b);
}

MULTINODE_TEST_NODE(RemotePortalTestNode, ClosurePropagationClient2) {
  IpczHandle b = ConnectToBroker();
  WaitForDirectRemoteLink(b);

  IpczHandle wait_for_c1;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&wait_for_c1, 1}));
  WaitForDirectRemoteLink(wait_for_c1);
  Put(b, "ok");

  IpczHandle ps[kNumPortalsForClosurePropagation];
  for (IpczHandle& p : ps) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));
  }

  CloseAll(ps);

  EXPECT_EQ("bye", WaitToGetString(b));
  Close(b);
}

MULTINODE_TEST(RemotePortalTest, ClosurePropagation) {
  // Regression test for https://crbug.com/1426471. The goal of this test is
  // to trigger a large number of parallel proxy bypasses to put pressure on
  // RouterLinkState allocation. Then we verify that we can still propagate
  // closure from end-to-end on each route, implying that the route didn't get
  // stuck halfway through a bypass.

  IpczHandle c1 = SpawnTestNode<ClosurePropagationClient1>();
  IpczHandle c2 = SpawnTestNode<ClosurePropagationClient2>();
  WaitForDirectRemoteLink(c1);
  WaitForDirectRemoteLink(c2);

  // Wait for the clients to establish a direct link so we start the real test
  // work in a consistent state.
  auto [wait_for_c1, wait_for_c2] = OpenPortals();
  Put(c1, "", {&wait_for_c2, 1});
  Put(c2, "", {&wait_for_c1, 1});
  EXPECT_EQ("ok", WaitToGetString(c1));
  EXPECT_EQ("ok", WaitToGetString(c2));

  for (size_t i = 0; i < kNumPortalsForClosurePropagation; ++i) {
    auto [q, p] = OpenPortals();
    Put(c1, "", {&q, 1});
    Put(c2, "", {&p, 1});
  }

  EXPECT_EQ("done", WaitToGetString(c1));
  Put(c1, "bye");
  Put(c2, "bye");
  CloseAll({c1, c2});
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

#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_DisconnectThroughProxy DISABLED_DisconnectThroughProxy
#else
#define MAYBE_DisconnectThroughProxy DisconnectThroughProxy
#endif
MULTINODE_TEST(RemotePortalTest, MAYBE_DisconnectThroughProxy) {
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

}  // namespace
}  // namespace ipcz
