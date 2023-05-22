// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace ipcz {
namespace {

using QueueingTestNode = test::TestNode;
using QueueingTest = test::MultinodeTest<QueueingTestNode>;

MULTINODE_TEST_NODE(QueueingTestNode, RemoteQueueFeedbackClient) {
  IpczHandle b = ConnectToBroker();

  // Wait for the first parcel to arrive.
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditions(b, {.flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
                                  .min_local_parcels = 0}));

  // Send and ack and wait for another parcel to arrive.
  absl::Notification new_parcel_arrived;
  EXPECT_EQ(IPCZ_RESULT_OK,
            Trap(b, {.flags = IPCZ_TRAP_NEW_LOCAL_PARCEL},
                 [&](const IpczTrapEvent&) { new_parcel_arrived.Notify(); }));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "ok"));
  new_parcel_arrived.WaitForNotification();

  std::string data;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &data));
  EXPECT_EQ("1234", data);

  std::string ack;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &ack));
  EXPECT_EQ("ok", ack);

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(b, IPCZ_TRAP_PEER_CLOSED));
  Close(b);
}

// Disabled because remote queue state monitoring has been temporarily dropped
// from ipcz to improve performance. See https://crbug.com/1383754.
MULTINODE_TEST(QueueingTest, DISABLED_RemoteQueueFeedback) {
  // Exercises operations which rely on feedback from the remote peer regarding
  // its inbound parcel queue state.
  IpczHandle c = SpawnTestNode<RemoteQueueFeedbackClient>();

  // This trap can only be set while the remote portal appears to be non-empty.
  const IpczTrapConditions all_bytes_consumed = {
      .size = sizeof(all_bytes_consumed),
      .flags = IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES,
      .max_remote_bytes = 1,
  };
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            Trap(c, all_bytes_consumed, [&](const auto&) {}));

  // Send 4 bytes and wait for acknowledgement that the parcel was received.
  std::string ack;
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "1234"));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, &ack));
  EXPECT_EQ("ok", ack);

  // Now these operations should always fail due to the specified limits.
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            PutWithLimits(c, {.max_queued_parcels = 1}, "meh"));
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            PutWithLimits(c, {.max_queued_bytes = 4}, "?"));

  // Now we should be able to install traps for both queued parcels and bytes on
  // the remote side.
  absl::Notification consumed_parcels;
  const IpczTrapConditions all_parcels_consumed = {
      .size = sizeof(all_parcels_consumed),
      .flags = IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS,
      .max_remote_parcels = 1,
  };
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      Trap(c, all_parcels_consumed, [&](const IpczTrapEvent& event) {
        EXPECT_TRUE(event.condition_flags & IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS);
        consumed_parcels.Notify();
      }));

  absl::Notification consumed_bytes;
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      Trap(c, all_bytes_consumed, [&](const IpczTrapEvent& event) {
        EXPECT_TRUE(event.condition_flags & IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES);
        consumed_bytes.Notify();
      }));

  // Ack back to the client so it will read its queue. Then we can wait for both
  // traps to notify.
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "ok"));
  consumed_parcels.WaitForNotification();
  consumed_bytes.WaitForNotification();

  // And now this Put operation should succeed.
  EXPECT_EQ(IPCZ_RESULT_OK,
            PutWithLimits(c, {.max_queued_parcels = 1, .max_queued_bytes = 4},
                          "meh!"));

  Close(c);
}

MULTINODE_TEST_NODE(QueueingTestNode, TwoPhaseQueueingClient) {
  IpczHandle b = ConnectToBroker();
  WaitForDirectRemoteLink(b);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, "go"));

  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditions(b, {.flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
                                  .min_local_parcels = 0}));
  size_t num_bytes;
  const void* data;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, &data,
                                            &num_bytes, nullptr));

  // The producer should only have been able to put 3 out of its 4 bytes.
  EXPECT_EQ("ipc",
            std::string_view(reinterpret_cast<const char*>(data), num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(b, num_bytes, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  Close(b);
}

// Disabled because remote queue state monitoring has been temporarily dropped
// from ipcz to improve performance. See https://crbug.com/1383754.
MULTINODE_TEST(QueueingTest, DISABLED_TwoPhaseQueueing) {
  IpczHandle c = SpawnTestNode<TwoPhaseQueueingClient>();
  WaitForDirectRemoteLink(c);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(c, &message));
  EXPECT_EQ("go", message);

  const IpczPutLimits limits = {
      .size = sizeof(limits),
      .max_queued_parcels = 1,
      .max_queued_bytes = 3,
  };

  size_t num_bytes = 4;
  void* data;
  const IpczBeginPutOptions options = {.size = sizeof(options),
                                       .limits = &limits};
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginPut(c, IPCZ_BEGIN_PUT_ALLOW_PARTIAL,
                                            &options, &num_bytes, &data));

  // There should not be enough space for all 4 bytes.
  EXPECT_EQ(3u, num_bytes);
  memcpy(data, "ipc", 3);
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndPut(c, data, num_bytes, nullptr, 0,
                                          IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED));
  Close(c);
}

MULTINODE_TEST_NODE(QueueingTestNode, TwoPhaseFeedbackClient) {
  IpczHandle b = ConnectToBroker();
  WaitForDirectRemoteLink(b);

  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditions(b, {.flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
                                  .min_local_parcels = 0}));
  size_t num_bytes;
  const void* data;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, &data,
                                            &num_bytes, nullptr));

  EXPECT_EQ("hello?",
            std::string_view(reinterpret_cast<const char*>(data), num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(b, num_bytes, 0, IPCZ_NO_FLAGS, nullptr, nullptr));
  Close(b);
}

// TODO(https://crbug.com/1361670): Fix flakiness and re-enable this test.
MULTINODE_TEST(QueueingTest, DISABLED_TwoPhaseFeedback) {
  IpczHandle c = SpawnTestNode<TwoPhaseFeedbackClient>();
  WaitForDirectRemoteLink(c);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "hello?"));
  EXPECT_EQ(IPCZ_RESULT_OK,
            WaitForConditions(c, {.flags = IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS,
                                  .max_remote_parcels = 1}));
  Close(c);
}

constexpr size_t kStressTestPortalCapacity = 256;
constexpr size_t kStressTestPayloadSize = 4 * 1024 * 1024;

MULTINODE_TEST_NODE(QueueingTestNode, RemoteQueueFeedbackStressTestClient) {
  IpczHandle b = ConnectToBroker();

  size_t bytes_received = 0;
  while (bytes_received < kStressTestPayloadSize) {
    // Consistency check: ensure that the portal never has more than
    // kStressTestPortalCapacity bytes available to retrieve. Otherwise limits
    // were not properly enforced by the sender.
    IpczPortalStatus status = {.size = sizeof(status)};
    EXPECT_EQ(IPCZ_RESULT_OK,
              ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
    EXPECT_LE(status.num_local_bytes, kStressTestPortalCapacity);

    const void* data;
    size_t num_bytes;
    const IpczResult begin_result =
        ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, &data, &num_bytes, nullptr);
    if (begin_result == IPCZ_RESULT_OK) {
      bytes_received += num_bytes;
      EXPECT_EQ(std::string_view(static_cast<const char*>(data), num_bytes),
                std::string(num_bytes, '!'));
      EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndGet(b, num_bytes, 0, IPCZ_NO_FLAGS,
                                              nullptr, nullptr));
      continue;
    }

    ASSERT_EQ(IPCZ_RESULT_UNAVAILABLE, begin_result);
    WaitForConditions(
        b, {.flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES, .min_local_bytes = 0});
  }

  Close(b);
}

// Disabled because remote queue state monitoring has been temporarily dropped
// from ipcz to improve performance. See https://crbug.com/1383754.
MULTINODE_TEST(QueueingTest, DISABLED_RemoteQueueFeedbackStressTest) {
  IpczHandle c = SpawnTestNode<RemoteQueueFeedbackStressTestClient>();

  size_t bytes_remaining = kStressTestPayloadSize;
  while (bytes_remaining) {
    void* data;
    size_t capacity = bytes_remaining;
    const IpczPutLimits limits = {
        .size = sizeof(limits),
        .max_queued_parcels = std::numeric_limits<size_t>::max(),
        .max_queued_bytes = kStressTestPortalCapacity,
    };
    const IpczBeginPutOptions options = {
        .size = sizeof(options),
        .limits = &limits,
    };
    const IpczResult begin_result = ipcz().BeginPut(
        c, IPCZ_BEGIN_PUT_ALLOW_PARTIAL, &options, &capacity, &data);
    if (begin_result == IPCZ_RESULT_OK) {
      size_t num_bytes = std::min(bytes_remaining, capacity);
      bytes_remaining -= num_bytes;
      memset(data, '!', num_bytes);
      EXPECT_EQ(IPCZ_RESULT_OK, ipcz().EndPut(c, data, num_bytes, nullptr, 0,
                                              IPCZ_NO_FLAGS, nullptr));
      continue;
    }
    ASSERT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED, begin_result);

    EXPECT_EQ(
        IPCZ_RESULT_OK,
        WaitForConditions(c, {.flags = IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES,
                              .max_remote_bytes = kStressTestPortalCapacity}));
  }

  WaitForConditionFlags(c, IPCZ_TRAP_PEER_CLOSED);
  Close(c);
}

}  // namespace
}  // namespace ipcz
