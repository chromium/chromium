// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string_view>

#include "ipcz/ipcz.h"
#include "reference_drivers/blob.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using Blob = reference_drivers::Blob;

using BoxTestNode = test::TestNode;
using BoxTest = test::MultinodeTest<BoxTestNode>;

IpczDriverHandle CreateTestBlob(std::string_view message) {
  return Blob::ReleaseAsHandle(MakeRefCounted<Blob>(message));
}

std::string GetBlobContents(IpczDriverHandle handle) {
  Ref<Blob> blob = Blob::TakeFromHandle(handle);
  return std::string(blob->message());
}

TEST_P(BoxTest, BoxAndUnbox) {
  constexpr const char kMessage[] = "Hello, world?";
  IpczDriverHandle blob_handle = CreateTestBlob(kMessage);

  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  blob_handle = IPCZ_INVALID_DRIVER_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
  EXPECT_EQ(kMessage, GetBlobContents(blob_handle));
}

TEST_P(BoxTest, CloseBox) {
  Ref<Blob> blob = MakeRefCounted<Blob>("meh");
  Ref<Blob::RefCountedFlag> destroyed = blob->destruction_flag_for_testing();
  IpczDriverHandle blob_handle = Blob::ReleaseAsHandle(std::move(blob));

  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  EXPECT_FALSE(destroyed->get());
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(box, IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(destroyed->get());
}

TEST_P(BoxTest, Peek) {
  constexpr const char kMessage[] = "Hello, world?";
  IpczDriverHandle blob_handle = CreateTestBlob(kMessage);
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  blob_handle = IPCZ_INVALID_DRIVER_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &blob_handle));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &blob_handle));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &blob_handle));

  Blob* blob = Blob::FromHandle(blob_handle);
  EXPECT_EQ(kMessage, blob->message());

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));

  Ref<Blob> released_blob = Blob::TakeFromHandle(blob_handle);
  EXPECT_EQ(blob, released_blob.get());
}

constexpr const char kMessage1[] = "Hello, world?";
constexpr const char kMessage2[] = "Hello, world!";

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxClient) {
  IpczHandle b = ConnectToBroker();

  std::string message;
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
  EXPECT_EQ(kMessage2, message);

  IpczDriverHandle blob_handle;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
  EXPECT_EQ(kMessage1, GetBlobContents(blob_handle));

  Close(b);
}

TEST_P(BoxTest, TransferBox) {
  IpczHandle c = SpawnTestNode<TransferBoxClient>();

  IpczDriverHandle blob_handle = CreateTestBlob(kMessage1);
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage2, {&box, 1}));

  Close(c);
}

constexpr size_t TransferBoxBetweenNonBrokersNumIterations = 50;

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient1) {
  IpczHandle q;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczDriverHandle blob_handle = CreateTestBlob(kMessage1);
    IpczHandle box;
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage2, {&box, 1}));
    box = IPCZ_INVALID_DRIVER_HANDLE;

    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message, {&box, 1}));
    EXPECT_EQ(kMessage1, message);
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
    EXPECT_EQ(kMessage2, GetBlobContents(blob_handle));
  }

  CloseAll({q, b});
}

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient2) {
  IpczHandle p;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczHandle box;
    IpczDriverHandle blob_handle;
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(p, &message, {&box, 1}));
    EXPECT_EQ(kMessage2, message);
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
    EXPECT_EQ(kMessage1, GetBlobContents(blob_handle));

    blob_handle = CreateTestBlob(kMessage2);
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(p, kMessage1, {&box, 1}));
  }

  CloseAll({p, b});
}

TEST_P(BoxTest, TransferBoxBetweenNonBrokers) {
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

INSTANTIATE_MULTINODE_TEST_SUITE_P(BoxTest);

}  // namespace
}  // namespace ipcz
