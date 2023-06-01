// Copyright 2022 The Chromium Authors
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

IpczTrapConditionFlags GetSatisfiedConditionsForUpdate(
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
  if ((conditions.flags & IPCZ_TRAP_NEW_LOCAL_PARCEL) &&
      reason == TrapSet::UpdateReason::kNewLocalParcel) {
    event_flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }
  return event_flags;
}

}  // namespace

TrapSet::TrapSet() = default;

TrapSet::~TrapSet() {
  ABSL_ASSERT(empty());
}

// static
IpczTrapConditionFlags TrapSet::GetSatisfiedConditions(
    const IpczTrapConditions& conditions,
    const IpczPortalStatus& current_status) {
  return GetSatisfiedConditionsForUpdate(conditions, UpdateReason::kInstallTrap,
                                         current_status);
}

IpczResult TrapSet::Add(const IpczTrapConditions& conditions,
                        IpczTrapEventHandler handler,
                        uintptr_t context,
                        const IpczPortalStatus& current_status,
                        IpczTrapConditionFlags* satisfied_condition_flags,
                        IpczPortalStatus* status) {
  last_known_status_ = current_status;
  IpczTrapConditionFlags flags =
      GetSatisfiedConditions(conditions, current_status);
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
  return IPCZ_RESULT_OK;
}

void TrapSet::UpdatePortalStatus(const OperationContext& context,
                                 const IpczPortalStatus& status,
                                 UpdateReason reason,
                                 TrapEventDispatcher& dispatcher) {
  last_known_status_ = status;
  for (auto* it = traps_.begin(); it != traps_.end();) {
    const Trap& trap = *it;
    IpczTrapConditionFlags flags =
        GetSatisfiedConditionsForUpdate(trap.conditions, reason, status);
    if (!flags) {
      ++it;
      continue;
    }

    if (context.is_api_call()) {
      flags |= IPCZ_TRAP_WITHIN_API_CALL;
    }
    dispatcher.DeferEvent(trap.handler, trap.context, flags, status);
    it = traps_.erase(it);
  }
}

void TrapSet::RemoveAll(const OperationContext& context,
                        TrapEventDispatcher& dispatcher) {
  IpczTrapConditionFlags flags = IPCZ_TRAP_REMOVED;
  if (context.is_api_call()) {
    flags |= IPCZ_TRAP_WITHIN_API_CALL;
  }
  for (const Trap& trap : traps_) {
    dispatcher.DeferEvent(trap.handler, trap.context, flags,
                          last_known_status_);
  }
  traps_.clear();
}

TrapSet::Trap::Trap(IpczTrapConditions conditions,
                    IpczTrapEventHandler handler,
                    uintptr_t context)
    : conditions(conditions), handler(handler), context(context) {}

TrapSet::Trap::~Trap() = default;

}  // namespace ipcz
