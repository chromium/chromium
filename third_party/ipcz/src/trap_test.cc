// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

#include "ipcz/ipcz.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test/test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"

namespace ipcz {
namespace {

class TrapTest : public test::Test {
 public:
  ~TrapTest() override { Close(node_); }

  std::pair<IpczHandle, IpczHandle> OpenPortals() {
    return TestBase::OpenPortals(node_);
  }

 private:
  const IpczHandle node_{CreateNode(reference_drivers::kSyncReferenceDriver)};
};

TEST_F(TrapTest, RemoveOnClose) {
  auto [a, b] = OpenPortals();

  // Install a few traps to watch for specific conditions. Both should instead
  // observe IPCZ_TRAP_REMOVED when the observed portal is closed.

  bool parcel_trap_removed = false;
  bool closure_trap_removed = false;

  IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_NEW_LOCAL_PARCEL,
  };
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_REMOVED | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              parcel_trap_removed = true;
            }));
  conditions.flags = IPCZ_TRAP_PEER_CLOSED;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_REMOVED | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              closure_trap_removed = true;
            }));

  EXPECT_FALSE(parcel_trap_removed);
  EXPECT_FALSE(closure_trap_removed);
  Close(b);
  EXPECT_TRUE(parcel_trap_removed);
  EXPECT_TRUE(closure_trap_removed);

  Close(a);
}

TEST_F(TrapTest, PeerClosed) {
  auto [a, b] = OpenPortals();

  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_PEER_CLOSED,
  };
  bool received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_PEER_CLOSED | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Close(a);
  EXPECT_TRUE(received_event);

  IpczTrapConditionFlags flags = IPCZ_NO_FLAGS;
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            Trap(
                b, conditions, [&](const IpczTrapEvent&) {}, &flags, &status));
  EXPECT_EQ(IPCZ_TRAP_PEER_CLOSED, flags);
  EXPECT_NE(IPCZ_NO_FLAGS, status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  Close(b);
}

TEST_F(TrapTest, MinLocalParcels) {
  auto [a, b] = OpenPortals();

  IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };
  bool received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(
                  IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_WITHIN_API_CALL,
                  e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Put(a, "beep beep");
  EXPECT_TRUE(received_event);

  // Can't install a new trap with the same conditions, because the minimum
  // local parcel count is already satisifed...
  IpczTrapConditionFlags flags = IPCZ_NO_FLAGS;
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            Trap(
                b, conditions, [&](const IpczTrapEvent&) {}, &flags, &status));
  EXPECT_EQ(IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS, flags);
  EXPECT_EQ(1u, status.num_local_parcels);

  // ...but we can still install a trap to watch for a larger number of parcels.
  received_event = false;
  conditions.min_local_parcels = 2;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(
                  IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_WITHIN_API_CALL,
                  e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Put(a, "hihihi");
  EXPECT_FALSE(received_event);
  Put(a, "okokok");
  EXPECT_TRUE(received_event);

  // Purge the messages. Should now be able to install another trap for > 0
  // parcels.
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &message));
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &message));
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &message));
  conditions.min_local_parcels = 0;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent&) {}));

  CloseAll({a, b});
}

TEST_F(TrapTest, MinLocalBytes) {
  auto [a, b] = OpenPortals();

  IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES,
      .min_local_bytes = 3,
  };
  bool received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(
                  IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES | IPCZ_TRAP_WITHIN_API_CALL,
                  e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Put(a, "w");
  EXPECT_FALSE(received_event);
  Put(a, "x");
  EXPECT_FALSE(received_event);
  Put(a, "y");
  EXPECT_FALSE(received_event);
  Put(a, "z");
  EXPECT_TRUE(received_event);

  IpczTrapConditionFlags flags = IPCZ_NO_FLAGS;
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            Trap(
                b, conditions, [&](const IpczTrapEvent&) {}, &flags, &status));
  EXPECT_EQ(IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES, flags);
  EXPECT_EQ(4u, status.num_local_bytes);

  CloseAll({a, b});
}

TEST_F(TrapTest, NewLocalParcel) {
  auto [a, b] = OpenPortals();

  IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_NEW_LOCAL_PARCEL,
  };
  bool received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_NEW_LOCAL_PARCEL | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Put(a, "beep beep");
  EXPECT_TRUE(received_event);

  received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_NEW_LOCAL_PARCEL | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              received_event = true;
            }));

  EXPECT_FALSE(received_event);
  Put(a, "beep beep beeeeep");
  EXPECT_TRUE(received_event);

  CloseAll({a, b});
}

TEST_F(TrapTest, DeadPortal) {
  auto [a, b] = OpenPortals();

  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_DEAD,
  };
  bool received_event = false;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_DEAD | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              received_event = true;
            }));

  // A "dead" signal can't be raised until the peer is closed AND all incoming
  // parcels have been drained.
  EXPECT_FALSE(received_event);
  Put(a, "goodbye");
  Close(a);
  EXPECT_FALSE(received_event);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &message));
  EXPECT_TRUE(received_event);

  IpczTrapConditionFlags flags = IPCZ_NO_FLAGS;
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            Trap(
                b, conditions, [&](const IpczTrapEvent&) {}, &flags, &status));
  EXPECT_EQ(IPCZ_TRAP_DEAD, flags);
  EXPECT_NE(IPCZ_NO_FLAGS, status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_NE(IPCZ_NO_FLAGS, status.flags & IPCZ_PORTAL_STATUS_DEAD);
  EXPECT_EQ(0u, status.num_local_parcels);

  Close(b);
}

TEST_F(TrapTest, MultipleTraps) {
  auto [a, b] = OpenPortals();

  // Install 3 separate traps. We'll trigger an event for only one of them
  // first, and then we'll trigger events for the other two simulataneously.
  bool observed_parcel = false;
  bool observed_closure = false;
  bool observed_death = false;
  IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_NEW_LOCAL_PARCEL,
  };
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_NEW_LOCAL_PARCEL | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              observed_parcel = true;
            }));

  conditions.flags = IPCZ_TRAP_PEER_CLOSED;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_PEER_CLOSED | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              observed_closure = true;
            }));

  conditions.flags = IPCZ_TRAP_DEAD;
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              EXPECT_EQ(IPCZ_TRAP_DEAD | IPCZ_TRAP_WITHIN_API_CALL,
                        e.condition_flags);
              observed_death = true;
            }));

  EXPECT_FALSE(observed_parcel);
  EXPECT_FALSE(observed_closure);
  EXPECT_FALSE(observed_death);

  Put(a, "hey");
  EXPECT_TRUE(observed_parcel);
  EXPECT_FALSE(observed_closure);
  EXPECT_FALSE(observed_death);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(b, &message));

  Close(a);
  EXPECT_TRUE(observed_parcel);
  EXPECT_TRUE(observed_closure);
  EXPECT_TRUE(observed_death);

  Close(b);
}

TEST_F(TrapTest, SyncTransportWithinAPICall) {
  const auto& kDriver = reference_drivers::kSyncReferenceDriver;
  const IpczHandle node_a = CreateNode(kDriver, IPCZ_CREATE_NODE_AS_BROKER);
  const IpczHandle node_b = CreateNode(kDriver);

  IpczDriverHandle t0, t1;
  kDriver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                           IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS, nullptr,
                           &t0, &t1);

  IpczHandle a, b;
  ipcz().ConnectNode(node_a, t0, 1, IPCZ_NO_FLAGS, nullptr, &a);
  ipcz().ConnectNode(node_b, t1, 1, IPCZ_CONNECT_NODE_TO_BROKER, nullptr, &b);

  bool received_event = false;
  IpczTrapConditionFlags event_flags = IPCZ_NO_FLAGS;
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
  };
  EXPECT_EQ(IPCZ_RESULT_OK, Trap(b, conditions, [&](const IpczTrapEvent& e) {
              received_event = true;
              event_flags = e.condition_flags;
            }));

  Put(a, "ping!");
  EXPECT_TRUE(received_event);

  // Even though the immediate source of the fired trap event was a transport
  // notification, the event should still be flagged as within an API call since
  // it happens while we're still in Put() above.
  EXPECT_TRUE(event_flags & IPCZ_TRAP_WITHIN_API_CALL);

  CloseAll({a, b, node_a, node_b});
}

}  // namespace
}  // namespace ipcz
