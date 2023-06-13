// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string_view>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

class ParcelTestNode : public test::TestNode {
 public:
  IpczResult WaitForParcels(IpczHandle portal, size_t count) {
    ABSL_HARDENING_ASSERT(count > 0);
    const IpczTrapConditions conditions = {
        .size = sizeof(conditions),
        .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
        .min_local_parcels = count - 1,
    };
    return WaitForConditions(portal, conditions);
  }

  IpczResult WaitForParcel(IpczHandle portal) {
    return WaitForParcels(portal, 1);
  }

  void SayGoodbye(IpczHandle portal) { Put(portal, "goodbye"); }

  void WaitForGoodbye(IpczHandle portal) {
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, &message));
    EXPECT_EQ("goodbye", message);
  }
};

using ParcelTest = test::MultinodeTest<ParcelTestNode>;

std::string_view StringFromData(const volatile void* data, size_t size) {
  return std::string_view{
      static_cast<const char*>(const_cast<const void*>(data)), size};
}

constexpr std::string_view kMessage = "here's that box of hornets you wanted";
constexpr std::string_view kHornets = "bzzzzz";

MULTINODE_TEST_NODE(ParcelTestNode, GetClient) {
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(b));

  // Retrieving a parcel object removes it from its portal's queue.
  IpczHandle parcel = 0;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_GET_PARTIAL, nullptr, nullptr,
                                       nullptr, nullptr, nullptr, &parcel));
  EXPECT_EQ(IPCZ_RESULT_UNAVAILABLE,
            ipcz().Get(b, IPCZ_GET_PARTIAL, nullptr, nullptr, nullptr, nullptr,
                       nullptr, &parcel));

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
  EXPECT_EQ(kMessage, std::string_view(buffer, num_bytes));
  EXPECT_EQ(1u, num_handles);

  // Handles are consumed by Get(), but data is not. We should no longer see
  // any handles, but should see the same data as before.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, buffer, &num_bytes, &box,
                       &num_handles, nullptr));
  EXPECT_EQ(kMessage.size(), num_bytes);
  EXPECT_EQ(kMessage, std::string_view(buffer, num_bytes));
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
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_GET_PARTIAL, nullptr, nullptr,
                                       nullptr, nullptr, nullptr, &parcel));

  // First validate feedback behavior for handle capacity requirements.
  size_t num_handles = 0;
  IpczTransaction transaction;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            nullptr, nullptr, &transaction));
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            nullptr, &num_handles, &transaction));

  IpczHandle box;
  const volatile void* data;
  size_t num_bytes;
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                            &box, &num_handles, &transaction));
  EXPECT_EQ(kMessage, StringFromData(data, num_bytes));
  EXPECT_EQ(1u, num_handles);

  // We can't start a new get of any kind during the two-phase get.
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                            &box, &num_handles, &transaction));
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().Get(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes, &box,
                       &num_handles, nullptr));

  // Two-phase gets on parcels can be aborted.
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      ipcz().EndGet(parcel, transaction, IPCZ_END_GET_ABORT, nullptr, nullptr));

  // We can restart a get transaction. The attached handle is now gone since it
  // was taken by the first BeginGet().
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes,
                            &box, &num_handles, &transaction));
  EXPECT_EQ(kMessage, StringFromData(data, num_bytes));
  EXPECT_EQ(0u, num_handles);

  // `box` is still valid from the original successful BeginGet() further above.
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
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_GET_PARTIAL, nullptr, nullptr,
                                       nullptr, nullptr, nullptr, &parcel));

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

MULTINODE_TEST_NODE(ParcelTestNode, SimpleParcelProvider) {
  IpczHandle b = ConnectToBroker();
  IpczHandle blob = BoxBlob(kHornets);
  auto [q, p] = OpenPortals();
  IpczHandle handles[] = {blob, p};
  Put(b, kMessage, handles);
  VerifyEndToEnd(q);
  WaitForGoodbye(b);
  CloseAll({b, q});
}

MULTINODE_TEST(ParcelTest, PartialTwoPhaseGetOnPortal) {
  IpczHandle c = SpawnTestNode<SimpleParcelProvider>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(c));

  IpczTransaction transaction;
  size_t num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(c, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            nullptr, &num_handles, &transaction));
  EXPECT_EQ(2u, num_handles);

  // Start a partial (empty) get and abort it. Should leave the parcel intact
  // and in queue.
  num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, nullptr, &num_handles, &transaction));
  EXPECT_EQ(0u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(c, transaction, IPCZ_END_GET_ABORT,
                                          nullptr, nullptr));

  // Now do a partial handle retrieval.
  IpczHandle box;
  num_handles = 1;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, &box, &num_handles, &transaction));
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(c, transaction, IPCZ_END_GET_ABORT,
                                          nullptr, nullptr));

  // Validate that we actually retrieved a handle, even in spite of the aborted
  // transaction.
  EXPECT_EQ(kHornets, UnboxBlob(box));

  // Now do another partial retrieval. We should only find one remaining handle,
  // a portal we can ping-pong.
  IpczHandle handles[4];
  num_handles = 4;
  const volatile void* data;
  size_t num_bytes;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_PARTIAL, nullptr, &data,
                            &num_bytes, handles, &num_handles, &transaction));
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(kMessage, StringFromData(data, num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(c, transaction, IPCZ_NO_FLAGS, nullptr, nullptr));

  const IpczHandle p = handles[0];
  VerifyEndToEnd(p);

  SayGoodbye(c);
  CloseAll({c, p});
}

MULTINODE_TEST(ParcelTest, PartialTwoPhaseGetOnParcel) {
  IpczHandle c = SpawnTestNode<SimpleParcelProvider>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcel(c));

  IpczHandle parcel;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(c, IPCZ_GET_PARTIAL, nullptr, nullptr,
                                       nullptr, nullptr, nullptr, &parcel));

  IpczTransaction transaction;
  size_t num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().BeginGet(parcel, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                            nullptr, &num_handles, &transaction));
  EXPECT_EQ(2u, num_handles);

  // Start a partial (empty) get and abort it. Should leave the parcel intact
  // and in queue.
  num_handles = 0;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(parcel, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, nullptr, &num_handles, &transaction));
  EXPECT_EQ(0u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(parcel, transaction, IPCZ_NO_FLAGS,
                                          nullptr, nullptr));

  // Now do a partial handle retrieval.
  IpczHandle box;
  num_handles = 1;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(parcel, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, &box, &num_handles, &transaction));
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(parcel, transaction, IPCZ_NO_FLAGS,
                                          nullptr, nullptr));

  // Validate that we actually retrieved a handle, even in spite of the aborted
  // transaction.
  EXPECT_EQ(kHornets, UnboxBlob(box));

  // Now do another partial retrieval. We should only find one remaining handle,
  // a portal we can ping-pong.
  IpczHandle handles[4];
  num_handles = 4;
  const volatile void* data;
  size_t num_bytes;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(parcel, IPCZ_BEGIN_GET_PARTIAL, nullptr, &data,
                            &num_bytes, handles, &num_handles, &transaction));
  EXPECT_EQ(1u, num_handles);
  EXPECT_EQ(kMessage, StringFromData(data, num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(parcel, transaction, IPCZ_NO_FLAGS,
                                          nullptr, nullptr));

  const IpczHandle p = handles[0];
  VerifyEndToEnd(p);

  SayGoodbye(c);
  CloseAll({c, p});
}

constexpr std::string_view kMessage1{"abcdef"};
constexpr std::string_view kMessage2{"12345678"};
constexpr std::string_view kMessage3{"whatever i guess"};

MULTINODE_TEST_NODE(ParcelTestNode, ProviderOfManyParcels) {
  IpczHandle b = ConnectToBroker();
  Put(b, kMessage1);
  Put(b, kMessage2);
  Put(b, kMessage3);
  WaitForGoodbye(b);
  CloseAll({b});
}

MULTINODE_TEST(ParcelTest, OverlappedTwoPhaseGets) {
  IpczHandle c = SpawnTestNode<ProviderOfManyParcels>();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForParcels(c, 3));

  // First begin a non-overlapped transaction and verify that we can't then
  // start another (overlapped or not).
  IpczTransaction t;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, nullptr, nullptr, &t));
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_PARTIAL, nullptr, nullptr,
                            nullptr, nullptr, nullptr, &t));
  EXPECT_EQ(
      IPCZ_RESULT_ALREADY_EXISTS,
      ipcz().BeginGet(c, IPCZ_BEGIN_GET_OVERLAPPED | IPCZ_BEGIN_GET_PARTIAL,
                      nullptr, nullptr, nullptr, nullptr, nullptr, &t));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(c, t, IPCZ_END_GET_ABORT, nullptr, nullptr));

  // Now start three concurrent transactions.
  IpczTransaction t1;
  const volatile void* data1;
  size_t size1;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_OVERLAPPED, nullptr, &data1,
                            &size1, nullptr, nullptr, &t1));
  IpczTransaction t2;
  const volatile void* data2;
  size_t size2;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_OVERLAPPED, nullptr, &data2,
                            &size2, nullptr, nullptr, &t2));
  IpczTransaction t3;
  const volatile void* data3;
  size_t size3;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginGet(c, IPCZ_BEGIN_GET_OVERLAPPED, nullptr, &data3,
                            &size3, nullptr, nullptr, &t3));

  EXPECT_EQ(kMessage1, StringFromData(data1, size1));
  EXPECT_EQ(kMessage2, StringFromData(data2, size2));
  EXPECT_EQ(kMessage3, StringFromData(data3, size3));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(c, t1, IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(c, t2, IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(c, t3, IPCZ_NO_FLAGS, nullptr, nullptr));

  SayGoodbye(c);
  CloseAll({c});
}

}  // namespace
}  // namespace ipcz
