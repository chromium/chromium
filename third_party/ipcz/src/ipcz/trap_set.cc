// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/trap_set.h"

#include <algorithm>
#include <utility>

#include "ipcz/ipcz.h"
#include "ipcz/trap_event_dispatcher.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

IpczTrapConditionFlags GetSatisfiedConditions(
    const IpczTrapConditions& conditions,
    TrapSet::UpdateReason reason,
    const IpczPortalStatus& status) {
  IpczTrapConditionFlags event_flags = 0;
  if ((conditions.flags & IPCZ_TRAP_PEER_CLOSED) &&
      (status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED)) {
    event_flags |= IPCZ_TRAP_PEER_CLOSED;
  }
  if ((conditions.flags & IPCZ_TRAP_DEAD) &&
      (status.flags & IPCZ_PORTAL_STATUS_DEAD)) {
    event_flags |= IPCZ_TRAP_DEAD;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS) &&
      status.num_local_parcels > conditions.min_local_parcels) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES) &&
      status.num_local_bytes > conditions.min_local_bytes) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES;
  }
  if ((conditions.flags & IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS) &&
      status.num_remote_parcels < conditions.max_remote_parcels) {
    event_flags |= IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS;
  }
  if ((conditions.flags & IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES) &&
      status.num_remote_bytes < conditions.max_remote_bytes) {
    event_flags |= IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES;
  }
  if ((conditions.flags & IPCZ_TRAP_NEW_LOCAL_PARCEL) &&
      reason == TrapSet::UpdateReason::kNewLocalParcel) {
    event_flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }
  return event_flags;
}

bool NeedRemoteState(IpczTrapConditionFlags flags) {
  return (flags & (IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS |
                   IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES)) != 0;
}

}  // namespace

TrapSet::TrapSet() = default;

TrapSet::~TrapSet() {
  ABSL_ASSERT(empty());
}

IpczResult TrapSet::Add(const IpczTrapConditions& conditions,
                        IpczTrapEventHandler handler,
                        uintptr_t context,
                        const IpczPortalStatus& current_status,
                        IpczTrapConditionFlags* satisfied_condition_flags,
                        IpczPortalStatus* status) {
  last_known_status_ = current_status;
  IpczTrapConditionFlags flags = GetSatisfiedConditions(
      conditions, UpdateReason::kInstallTrap, current_status);
  if (flags != 0) {
    if (satisfied_condition_flags) {
      *satisfied_condition_flags = flags;
    }
    if (status) {
      // Note that we copy the minimum number of bytes between the size of our
      // IpczPortalStatus and the size of the caller's, which may differ if
      // coming from another version of ipcz. The `size` field is updated to
      // reflect how many bytes are actually meaningful here.
      const uint32_t size = std::min(status->size, sizeof(current_status));
      memcpy(status, &current_status, size);
      status->size = size;
    }
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  traps_.emplace_back(conditions, handler, context);
  if (NeedRemoteState(conditions.flags)) {
    ++num_traps_monitoring_remote_state_;
  }
  return IPCZ_RESULT_OK;
}

void TrapSet::UpdatePortalStatus(const IpczPortalStatus& status,
                                 UpdateReason reason,
                                 TrapEventDispatcher& dispatcher) {
  last_known_status_ = status;
  for (auto* it = traps_.begin(); it != traps_.end();) {
    const Trap& trap = *it;
    const IpczTrapConditionFlags flags =
        GetSatisfiedConditions(trap.conditions, reason, status);
    if (!flags) {
      ++it;
      continue;
    }

    dispatcher.DeferEvent(trap.handler, trap.context, flags, status);
    it = traps_.erase(it);
    if (NeedRemoteState(flags)) {
      --num_traps_monitoring_remote_state_;
    }
  }
}

void TrapSet::RemoveAll(TrapEventDispatcher& dispatcher) {
  for (const Trap& trap : traps_) {
    dispatcher.DeferEvent(trap.handler, trap.context, IPCZ_TRAP_REMOVED,
                          last_known_status_);
  }
  traps_.clear();
  num_traps_monitoring_remote_state_ = 0;
}

TrapSet::Trap::Trap(IpczTrapConditions conditions,
                    IpczTrapEventHandler handler,
                    uintptr_t context)
    : conditions(conditions), handler(handler), context(context) {}

TrapSet::Trap::~Trap() = default;

}  // namespace ipcz
