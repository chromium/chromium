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

TrapSet::TrapSet() = default;

TrapSet::~TrapSet() {
  ABSL_ASSERT(empty());
}

IpczResult TrapSet::Add(const IpczTrapConditions& conditions,
                        IpczTrapEventHandler handler,
                        uintptr_t context,
                        IpczPortalStatusFlags status_flags,
                        size_t num_local_parcels,
                        size_t num_local_bytes,
                        IpczTrapConditionFlags* satisfied_condition_flags,
                        IpczPortalStatus* status) {
  last_known_status_.flags = status_flags;
  last_known_status_.num_local_parcels = num_local_parcels;
  last_known_status_.num_local_bytes = num_local_bytes;
  IpczTrapConditionFlags flags =
      GetSatisfiedConditionsForUpdate(conditions, UpdateReason::kInstallTrap);
  if (flags != 0) {
    if (satisfied_condition_flags) {
      *satisfied_condition_flags = flags;
    }
    if (status) {
      // Note that we copy the minimum number of bytes between the size of our
      // IpczPortalStatus and the size of the caller's, which may differ if
      // coming from another version of ipcz. The `size` field is updated to
      // reflect how many bytes are actually meaningful here.
      const uint32_t size = std::min(status->size, sizeof(last_known_status_));
      memcpy(status, &last_known_status_, size);
      status->size = size;
    }
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  traps_.emplace_back(conditions, handler, context);
  return IPCZ_RESULT_OK;
}

void TrapSet::NotifyNewLocalParcel(const OperationContext& context,
                                   IpczPortalStatusFlags status_flags,
                                   size_t num_local_parcels,
                                   size_t num_local_bytes,
                                   TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(context, status_flags, num_local_parcels, num_local_bytes,
                     UpdateReason::kNewLocalParcel, dispatcher);
}

void TrapSet::NotifyLocalParcelConsumed(const OperationContext& context,
                                        IpczPortalStatusFlags status_flags,
                                        size_t num_local_parcels,
                                        size_t num_local_bytes,
                                        TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(context, status_flags, num_local_parcels, num_local_bytes,
                     UpdateReason::kLocalParcelConsumed, dispatcher);
}

void TrapSet::NotifyPeerClosed(const OperationContext& context,
                               IpczPortalStatusFlags status_flags,
                               TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(context, status_flags,
                     last_known_status_.num_local_parcels,
                     last_known_status_.num_local_bytes,
                     UpdateReason::kPeerClosed, dispatcher);
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

IpczTrapConditionFlags TrapSet::GetSatisfiedConditionsForUpdate(
    const IpczTrapConditions& conditions,
    TrapSet::UpdateReason reason) {
  IpczTrapConditionFlags event_flags = 0;
  if ((conditions.flags & IPCZ_TRAP_PEER_CLOSED) &&
      (last_known_status_.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED)) {
    event_flags |= IPCZ_TRAP_PEER_CLOSED;
  }
  if ((conditions.flags & IPCZ_TRAP_DEAD) &&
      (last_known_status_.flags & IPCZ_PORTAL_STATUS_DEAD)) {
    event_flags |= IPCZ_TRAP_DEAD;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS) &&
      last_known_status_.num_local_parcels > conditions.min_local_parcels) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES) &&
      last_known_status_.num_local_bytes > conditions.min_local_bytes) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES;
  }
  if ((conditions.flags & IPCZ_TRAP_NEW_LOCAL_PARCEL) &&
      reason == UpdateReason::kNewLocalParcel) {
    event_flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }
  return event_flags;
}

void TrapSet::UpdatePortalStatus(const OperationContext& context,
                                 IpczPortalStatusFlags status_flags,
                                 size_t num_local_parcels,
                                 size_t num_local_bytes,
                                 UpdateReason reason,
                                 TrapEventDispatcher& dispatcher) {
  last_known_status_.flags = status_flags;
  last_known_status_.num_local_parcels = num_local_parcels;
  last_known_status_.num_local_bytes = num_local_bytes;
  for (auto* it = traps_.begin(); it != traps_.end();) {
    const Trap& trap = *it;
    IpczTrapConditionFlags flags =
        GetSatisfiedConditionsForUpdate(trap.conditions, reason);
    if (!flags) {
      ++it;
      continue;
    }

    if (context.is_api_call()) {
      flags |= IPCZ_TRAP_WITHIN_API_CALL;
    }
    dispatcher.DeferEvent(trap.handler, trap.context, flags,
                          last_known_status_);
    it = traps_.erase(it);
  }
}

TrapSet::Trap::Trap(IpczTrapConditions conditions,
                    IpczTrapEventHandler handler,
                    uintptr_t context)
    : conditions(conditions), handler(handler), context(context) {}

TrapSet::Trap::~Trap() = default;

}  // namespace ipcz
