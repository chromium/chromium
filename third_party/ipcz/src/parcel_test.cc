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

class ParcelTestNode : public test::TestNode {
 public:
  IpczResult WaitForParcel(IpczHandle portal) {
    const IpczTrapConditions conditions = {
        .size = sizeof(conditions),
        .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
        .min_local_parcels = 0,
    };
    return WaitForConditions(portal, conditions);
  }
};

using ParcelTest = test::MultinodeTest<ParcelTestNode>;

constexpr std::string_view kMessage = "here's that box of hornets you wanted";
constexpr std::string_view kHornets = "bzzzzz";

MULTINODE_TEST_NODE(ParcelTestNode, GetClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(b));

  // Retrieving a parcel object removes it from its portal's queue.
  IpczHandle parcel = 0;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(b, IPCZ_GET_PARCEL_ONLY, nullptr, nullptr, nullptr,
                       nullptr, nullptr, &parcel));
  EXPECT_EQ(IPCZ_RESULT_UNAVAILABLE,
            ipcz().Get(b, IPCZ_GET_PARCEL_ONLY, nullptr, nullptr, nullptr,
                       nullptr, nullptr, &parcel));

  // Short reads behave as with portals, providing parcel dimensions on output.
  size_t num_bytes = 0;
  size_t num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                       nullptr, &num_handles, nullptr));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(1u, num_handles);

  // Invalid arguments: null data or handles with non-zero capacity.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                       nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                       nullptr, &num_handles, nullptr));

  // Verify the contents.
  char buffer[kMessage.size()];
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, buffer, &num_bytes, &box,
                       &num_handles, nullptr));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(kMessage, std::string_view(buffer, num_bytes));

  // Contents of the parcel are consumed by Get(), so now there's nothing left.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, buffer, &num_bytes, &box,
                       &num_handles, nullptr));
  EXPECT_EQ(0u, num_bytes);
  EXPECT_EQ(0u, num_handles);

  // Send the contents of the box back as another parcel.
  Put(b, UnboxBlob(box));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_DEAD));
  CloseAll({b, parcel});
}

MULTINODE_TEST(ParcelTest, Get) {
  IpczHandle c = SpawnTestNode<GetClient>();

  IpczHandle blob = BoxBlob(kHornets);
  Put(c, kMessage, {&blob, 1});

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, &message));
  EXPECT_EQ(kHornets, message);
  Close(c);
}

MULTINODE_TEST_NODE(ParcelTestNode, TwoPhaseGetClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(b));

  IpczHandle parcel = 0;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(b, IPCZ_GET_PARCEL_ONLY, nullptr, nullptr, nullptr,
                       nullptr, nullptr, &parcel));

  // Various combinations of missing args return to indicate size requirements.
  const void* data;
  size_t num_bytes;
  size_t num_handles;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            nullptr));
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                            nullptr));
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                            &num_handles));
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            &num_handles));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr,
                                            &data, &num_bytes, &num_handles));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(1u, num_handles);

  // We can't start a new get of any kind during the two-phase get.
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                            &num_handles));
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                       nullptr, &num_handles, nullptr));

  // Two-phase gets on parcels can be aborted.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(parcel, 0, 0, IPCZ_END_GET_ABORT, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr,
                                            &data, &num_bytes, &num_handles));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(1u, num_handles);

  // Verify the contents, and partially consume the parcel.
  IpczHandle box;
  EXPECT_EQ(kMessage,
            std::string_view(static_cast<const char*>(data), num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(parcel, 1, 1, IPCZ_NO_FLAGS, nullptr, &box));

  // A new two-phase read should see the first byte gone as well as the box.
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr,
                                            &data, &num_bytes, &num_handles));
  EXPECT_EQ(kMessage.size() - 1, num_bytes);
  EXPECT_EQ(kMessage.substr(1),
            std::string_view(static_cast<const char*>(data), num_bytes));
  EXPECT_EQ(0u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(parcel, 0, 0, IPCZ_END_GET_ABORT, nullptr, nullptr));

  // Send the contents of the box back as another parcel.
  Put(b, UnboxBlob(box));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_DEAD));
  CloseAll({b, parcel});
}

MULTINODE_TEST(ParcelTest, TwoPhaseGet) {
  IpczHandle c = SpawnTestNode<TwoPhaseGetClient>();

  IpczHandle blob = BoxBlob(kHornets);
  Put(c, kMessage, {&blob, 1});

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, &message));
  EXPECT_EQ(kHornets, message);
  Close(c);
}

MULTINODE_TEST_NODE(ParcelTestNode, CloseClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle parcel;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(b));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(b, IPCZ_GET_PARCEL_ONLY, nullptr, nullptr, nullptr,
                       nullptr, nullptr, &parcel));

  size_t num_bytes = 0;
  size_t num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                       nullptr, &num_handles, nullptr));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(1u, num_handles);

  // Closing the parcel should close any attached objects. The broker should
  // observer its `q` portal dying, because that portal's peer was attached to
  // this parcel.
  Close(parcel);

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_DEAD));
  Close(b);
}

MULTINODE_TEST(ParcelTest, Close) {
  IpczHandle c = SpawnTestNode<CloseClient>();
  auto [q, p] = OpenPortals();
  Put(c, kMessage, {&p, 1});

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(q, IPCZ_TRAP_DEAD));
  CloseAll({c, q});
}

}  // namespace
}  // namespace ipcz
