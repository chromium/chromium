// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string_view>

#include "ipcz/ipcz.h"
#include "reference_drivers/blob.h"
#include "reference_drivers/memory.h"
#include "reference_drivers/os_handle.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using BoxTestNode = test::TestNode;
using BoxTest = test::MultinodeTest<BoxTestNode>;

using Blob = reference_drivers::Blob;

// Creates a test driver Blob object with an inlined data payload and a shared
// memory object with an embedded message.
IpczDriverHandle CreateTestBlob(std::string_view inline_message,
                                std::string_view shm_message) {
  reference_drivers::Memory memory(shm_message.size());
  auto mapping = memory.Map();
  memcpy(mapping.base(), shm_message.data(), shm_message.size());
  reference_drivers::OSHandle memory_handle = memory.TakeHandle();
  return Blob::ReleaseAsHandle(
      MakeRefCounted<Blob>(inline_message, absl::MakeSpan(&memory_handle, 1)));
}

bool BlobContentsMatch(IpczDriverHandle blob_handle,
                       std::string_view expected_inline_message,
                       std::string_view expected_shm_message) {
  Ref<Blob> blob = Blob::TakeFromHandle(blob_handle);
  if (expected_inline_message != blob->message()) {
    return false;
  }

  ABSL_ASSERT(blob->handles().size() == 1);
  ABSL_ASSERT(blob->handles()[0].is_valid());
  reference_drivers::Memory memory = reference_drivers::Memory(
      std::move(blob->handles()[0]), expected_shm_message.size());

  auto new_mapping = memory.Map();
  if (expected_shm_message != std::string_view(new_mapping.As<char>())) {
    return false;
  }

  return true;
}

TEST_P(BoxTest, BoxAndUnbox) {
  constexpr const char kMessage[] = "Hello, world?";
  IpczDriverHandle blob_handle =
      Blob::ReleaseAsHandle(MakeRefCounted<Blob>(kMessage));

  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  blob_handle = IPCZ_INVALID_DRIVER_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));

  Ref<Blob> blob = Blob::TakeFromHandle(blob_handle);
  EXPECT_EQ(kMessage, blob->message());
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
  IpczDriverHandle blob_handle =
      Blob::ReleaseAsHandle(MakeRefCounted<Blob>(kMessage));
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
constexpr const char kMessage3[] = "Hello! World!";

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxClient) {
  IpczHandle b = ConnectToBroker();

  std::string message;
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
  EXPECT_EQ(kMessage3, message);

  IpczDriverHandle blob_handle;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
  EXPECT_TRUE(BlobContentsMatch(blob_handle, kMessage1, kMessage2));

  Close(b);
}

TEST_P(BoxTest, TransferBox) {
  IpczHandle c = SpawnTestNode<TransferBoxClient>();

  IpczDriverHandle blob_handle = CreateTestBlob(kMessage1, kMessage2);
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));

  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage3, {&box, 1}));

  Close(c);
}

constexpr size_t TransferBoxBetweenNonBrokersNumIterations = 50;

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient1) {
  IpczHandle q;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczDriverHandle blob_handle = CreateTestBlob(kMessage1, kMessage2);
    IpczHandle box;
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Box(node(), blob_handle, IPCZ_NO_FLAGS, nullptr, &box));
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage3, {&box, 1}));
    box = IPCZ_INVALID_DRIVER_HANDLE;

    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message, {&box, 1}));
    EXPECT_EQ(kMessage1, message);
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
    EXPECT_TRUE(BlobContentsMatch(blob_handle, kMessage2, kMessage3));
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
    EXPECT_EQ(kMessage3, message);
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &blob_handle));
    EXPECT_TRUE(BlobContentsMatch(blob_handle, kMessage1, kMessage2));

    blob_handle = CreateTestBlob(kMessage2, kMessage3);
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
