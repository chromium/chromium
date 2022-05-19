// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

using RemotePortalTest = test::MultinodeTestWithDriver;

TEST_P(RemotePortalTest, BasicConnection) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();
  auto [a, b] = ConnectBrokerToNonBroker(broker, non_broker);

  VerifyEndToEnd(a, b);

  CloseAll({a, b, non_broker, broker});
}

TEST_P(RemotePortalTest, PortalTransfer) {
  IpczHandle broker = CreateBrokerNode();
  IpczHandle non_broker = CreateNonBrokerNode();
  auto [a, b] = ConnectBrokerToNonBroker(broker, non_broker);
  auto [c, d] = OpenPortals(broker);

  // Send portal `d` to the non-broker node.
  const std::string kMessage = "hello";
  EXPECT_EQ(IPCZ_RESULT_OK, Put(a, kMessage, {&d, 1}));
  d = IPCZ_INVALID_HANDLE;

  // Retrieve portal `d` from the sent parcel.
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&d, 1}));
  EXPECT_EQ(kMessage, message);
  EXPECT_NE(IPCZ_INVALID_HANDLE, d);

  // Portals `c` and `d` should be able to communicate end-to-end across the
  // node boundary.
  VerifyEndToEnd(c, d);

  CloseAll({a, b, c, d, non_broker, broker});
}

TEST_P(RemotePortalTest, MultipleHops) {
  IpczHandle node0 = CreateBrokerNode();
  IpczHandle node1 = CreateNonBrokerNode();
  IpczHandle node2 = CreateNonBrokerNode();

  auto [a, b] = ConnectBrokerToNonBroker(node0, node1);
  auto [c, d] = ConnectBrokerToNonBroker(node0, node2);
  auto [e, f] = OpenPortals(node1);

  // Send `f` from node1 to node0 and then from node0 to node2
  Put(b, "here", {&f, 1});
  f = IPCZ_INVALID_HANDLE;

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(a, &message, {&f, 1}));
  ASSERT_NE(IPCZ_INVALID_HANDLE, f);

  Put(c, "ok ok", {&f, 1});
  f = IPCZ_INVALID_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(d, &message, {&f, 1}));
  ASSERT_NE(IPCZ_INVALID_HANDLE, f);

  constexpr size_t kNumIterations = 100;
  for (size_t i = 0; i < kNumIterations; ++i) {
    Put(e, "merp");
    Put(f, "nerp");
  }
  for (size_t i = 0; i < kNumIterations; ++i) {
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(f, &message));
    EXPECT_EQ("merp", message);
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(e, &message));
    EXPECT_EQ("nerp", message);
  }

  CloseAll({a, b, c, d, e, f, node2, node1, node0});
}

TEST_P(RemotePortalTest, TransferBackAndForth) {
  IpczHandle node0 = CreateBrokerNode();
  IpczHandle node1 = CreateNonBrokerNode();

  auto [a, b] = ConnectBrokerToNonBroker(node0, node1);
  auto [c, d] = OpenPortals(node0);

  std::string message;
  constexpr size_t kNumIterations = 8;
  for (size_t i = 0; i < kNumIterations; ++i) {
    Put(c, "hi");
    Put(a, "", {&d, 1});
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&d, 1}));
    Put(b, "", {&d, 1});
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(a, &message, {&d, 1}));
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(d, &message));
    EXPECT_EQ("hi", message);
  }

  VerifyEndToEnd(c, d);

  CloseAll({a, b, c, d, node1, node0});
}

INSTANTIATE_MULTINODE_TEST_SUITE_P(RemotePortalTest);

}  // namespace
}  // namespace ipcz
