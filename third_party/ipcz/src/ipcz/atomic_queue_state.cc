// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/atomic_queue_state.h"

#include <cstdint>

#include "ipcz/monitored_atomic.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

AtomicQueueState::AtomicQueueState() noexcept = default;

AtomicQueueState::QueryResult AtomicQueueState::Query(
    const MonitorSelection& monitors) {
  return {
      .num_parcels_consumed =
          num_parcels_consumed_.Query({.monitor = monitors.monitor_parcels}),
      .num_bytes_consumed =
          num_bytes_consumed_.Query({.monitor = monitors.monitor_bytes}),
  };
}

bool AtomicQueueState::Update(const UpdateValue& value) {
  ABSL_ASSERT(value.num_parcels_consumed <=
              MonitoredAtomic<uint64_t>::kMaxValue);
  ABSL_ASSERT(value.num_bytes_consumed <= MonitoredAtomic<uint64_t>::kMaxValue);
  const bool parcels_were_monitored =
      num_parcels_consumed_.UpdateValueAndResetMonitor(
          value.num_parcels_consumed);
  const bool bytes_were_monitored =
      num_bytes_consumed_.UpdateValueAndResetMonitor(value.num_bytes_consumed);
  return parcels_were_monitored || bytes_were_monitored;
}

}  // namespace ipcz
