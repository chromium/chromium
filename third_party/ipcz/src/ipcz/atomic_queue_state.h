// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ATOMIC_QUEUE_STATE_
#define IPCZ_SRC_IPCZ_ATOMIC_QUEUE_STATE_

#include <cstdint>
#include <type_traits>

#include "ipcz/monitored_atomic.h"

namespace ipcz {

// AtomicQueueState holds some trivial data about how much of a router's inbound
// parcel sequence has been consumed so far.
//
// Note that the fields herein are not strictly synchronized. If a queue
// accumulates a 4k parcel and an 8k parcel which are both then consumed by the
// application, the remote sender may observe `num_parcels_consumed` at 0, then
// 1, then 2; and they may observe `num_bytes_consumed` at 0, then 4k, and then
// 12k; the ordering of those individual progressions is guaranteed, but there's
// no guarantee that an observer will see `num_parcels_consumed` as 1 at the
// same time they see `num_bytes_consumed` as 4k.
class alignas(8) AtomicQueueState {
 public:
  AtomicQueueState() noexcept;

  // Performs a best-effort query of the most recently visible value on both
  // fields and returns them as a QueryResult. `monitors` determines whether
  // each field will be atomically marked for monitoring at the same time its
  // value is retrieved.
  struct QueryResult {
    MonitoredAtomic<uint64_t>::State num_parcels_consumed;
    MonitoredAtomic<uint64_t>::State num_bytes_consumed;
  };
  struct MonitorSelection {
    bool monitor_parcels;
    bool monitor_bytes;
  };
  QueryResult Query(const MonitorSelection& monitors);

  // Updates both fields with new values, resetting any monitor bit that may
  // have been set on either one. If either field had a monitor bit set prior to
  // this update, this returns true. Otherwise it returns false.
  struct UpdateValue {
    uint64_t num_parcels_consumed;
    uint64_t num_bytes_consumed;
  };
  bool Update(const UpdateValue& value);

 private:
  // The number of parcels consumed from the router's inbound parcel queue,
  // either by the application reading from its portal, or by ipcz proxying them
  // onward to another router.
  MonitoredAtomic<uint64_t> num_parcels_consumed_{0};

  // The total number of bytes of data consumed from the router's inbound parcel
  // queue. This is the sum of the data size of all parcels covered by
  // `consumed_sequence_length`, plus any bytes already consumed from the
  // next parcel in sequence if it's been partially consumed..
  MonitoredAtomic<uint64_t> num_bytes_consumed_{0};
};

// This must remain stable at 16 bytes in size, as it's part of shared memory
// layouts. Trivial copyability is also required as a proxy condition to prevent
// changes which might break that usage (e.g. introduction of a non-trivial
// destructor.)
static_assert(sizeof(AtomicQueueState) == 16, "Invalid AtomicQueueState size");
static_assert(std::is_trivially_copyable_v<AtomicQueueState>,
              "AtomicQueueState must be trivially copyable");

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ATOMIC_QUEUE_STATE_
