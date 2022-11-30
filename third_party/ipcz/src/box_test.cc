// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string_view>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using BoxTestNode = test::TestNode;
using BoxTest = test::MultinodeTest<BoxTestNode>;

MULTINODE_TEST(BoxTest, BoxAndUnbox) {
  constexpr const char kMessage[] = "Hello, world?";
  EXPECT_EQ(kMessage, UnboxBlob(BoxBlob(kMessage)));
}

MULTINODE_TEST(BoxTest, CloseBox) {
  // Verifies that box closure releases its underlying driver object. This test
  // does not explicitly observe side effects of that release, but LSan will
  // fail if something's off.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Close(BoxBlob("meh"), IPCZ_NO_FLAGS, nullptr));
}

MULTINODE_TEST(BoxTest, Peek) {
  constexpr std::string_view kMessage = "Hello, world?";
  IpczHandle box = BoxBlob(kMessage);

  IpczDriverHandle memory = IPCZ_INVALID_DRIVER_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &memory));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &memory));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &memory));
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, memory);

  IpczDriverHandle mapping;
  void* base;
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().MapSharedMemory(memory, IPCZ_NO_FLAGS, nullptr, &base,
                                        &mapping));
  std::string contents(static_cast<const char*>(base), kMessage.size());
  EXPECT_EQ(kMessage, contents);
  EXPECT_EQ(IPCZ_RESULT_OK, GetDriver().Close(mapping, IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(kMessage, UnboxBlob(box));
}

constexpr const char kMessage1[] = "Hello, world?";
constexpr const char kMessage2[] = "Hello, world!";
constexpr const char kMessage3[] = "Hello. World.";

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxClient) {
  IpczHandle b = ConnectToBroker();

  std::string message;
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(kMessage1, UnboxBlob(box));
  Close(b);
}

MULTINODE_TEST(BoxTest, TransferBox) {
  IpczHandle c = SpawnTestNode<TransferBoxClient>();
  IpczHandle box = BoxBlob(kMessage1);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage2, {&box, 1}));
  Close(c);
}

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxAndPortalClient) {
  IpczHandle b = ConnectToBroker();

  IpczHandle handles[2];
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, handles));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(handles[1], kMessage3));
  EXPECT_EQ(kMessage1, UnboxBlob(handles[0]));
  CloseAll({b, handles[1]});
}

MULTINODE_TEST(BoxTest, TransferBoxAndPortal) {
  IpczHandle c = SpawnTestNode<TransferBoxAndPortalClient>();

  auto [q, p] = OpenPortals();
  IpczHandle box = BoxBlob(kMessage1);
  IpczHandle handles[] = {box, p};
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage2, absl::MakeSpan(handles)));

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message));
  EXPECT_EQ(kMessage3, message);
  CloseAll({c, q});
}

constexpr size_t TransferBoxBetweenNonBrokersNumIterations = 50;

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient1) {
  IpczHandle q;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczHandle box = BoxBlob(kMessage1);
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage2, {&box, 1}));
    box = IPCZ_INVALID_DRIVER_HANDLE;

    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message, {&box, 1}));
    EXPECT_EQ(kMessage1, message);
    EXPECT_EQ(kMessage2, UnboxBlob(box));
  }

  WaitForDirectRemoteLink(q);
  CloseAll({q, b});
}

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient2) {
  IpczHandle p;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczHandle box;
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(p, &message, {&box, 1}));
    EXPECT_EQ(kMessage2, message);
    EXPECT_EQ(kMessage1, UnboxBlob(box));

    box = BoxBlob(kMessage2);
    EXPECT_EQ(IPCZ_RESULT_OK, Put(p, kMessage1, {&box, 1}));
  }

  WaitForDirectRemoteLink(p);
  CloseAll({p, b});
}

MULTINODE_TEST(BoxTest, TransferBoxBetweenNonBrokers) {
  IpczHandle c1 = SpawnTestNode<TransferBoxBetweenNonBrokersClient1>();
  IpczHandle c2 = SpawnTestNode<TransferBoxBetweenNonBrokersClient2>();

  // Create a new portal pair and send each end to one of the two non-brokers so
  // they'll establish a direct link.
  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  // Wait for the clients to finish their business and go away.
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c1, IPCZ_TRAP_PEER_CLOSED));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c2, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({c1, c2});
}

}  // namespace
}  // namespace ipcz
