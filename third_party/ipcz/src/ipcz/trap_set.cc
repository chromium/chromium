// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/trap_set.h"

#include <algorithm>
#include <utility>

#include "ipcz/api_context.h"
#include "ipcz/ipcz.h"
#include "ipcz/trap_event_dispatcher.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"
#include "util/safe_math.h"

namespace ipcz {

TrapSet::TrapSet() = default;

TrapSet::~TrapSet() {
  ABSL_ASSERT(empty());
}

IpczResult TrapSet::Add(const IpczTrapConditions& conditions,
                        IpczTrapEventHandler handler,
                        uintptr_t context,
                        IpczPortalStatusFlags status_flags,
                        ParcelQueue& inbound_parcel_queue,
                        IpczTrapConditionFlags* satisfied_condition_flags,
                        IpczPortalStatus* status) {
  IpczTrapConditionFlags flags = GetSatisfiedConditionsForUpdate(
      conditions, status_flags, inbound_parcel_queue,
      UpdateReason::kInstallTrap);
  if (flags != 0) {
    if (satisfied_condition_flags) {
      *satisfied_condition_flags = flags;
    }
    if (status) {
      // The `size` field is updated to reflect how many bytes are actually
      // meaningful here.
      const uint32_t size = std::min(status->size, sizeof(IpczPortalStatus));
      status->size = size;
      status->flags = status_flags;
      status->num_local_parcels =
          inbound_parcel_queue.GetNumAvailableElements();
      status->num_local_bytes =
          inbound_parcel_queue.GetTotalAvailableElementSize();
    }
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  traps_.emplace_back(conditions, handler, context);
  return IPCZ_RESULT_OK;
}

void TrapSet::NotifyNewLocalParcel(IpczPortalStatusFlags status_flags,
                                   ParcelQueue& inbound_parcel_queue,
                                   TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(status_flags, inbound_parcel_queue,
                     UpdateReason::kNewLocalParcel, dispatcher);
}

void TrapSet::NotifyLocalParcelConsumed(IpczPortalStatusFlags status_flags,
                                        ParcelQueue& inbound_parcel_queue,
                                        TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(status_flags, inbound_parcel_queue,
                     UpdateReason::kLocalParcelConsumed, dispatcher);
}

void TrapSet::NotifyPeerClosed(IpczPortalStatusFlags status_flags,
                               ParcelQueue& inbound_parcel_queue,
                               TrapEventDispatcher& dispatcher) {
  UpdatePortalStatus(status_flags, inbound_parcel_queue,
                     UpdateReason::kPeerClosed, dispatcher);
}

void TrapSet::RemoveAll(TrapEventDispatcher& dispatcher) {
  IpczTrapConditionFlags flags = IPCZ_TRAP_REMOVED;
  if (APIContext::IsCurrentThreadWithinAPICall()) {
    flags |= IPCZ_TRAP_WITHIN_API_CALL;
  }

  // Forced trap removal implies the portal has been invalidated by closure or
  // transfer. In any case, this status is meaningless.
  const IpczPortalStatus status{
      .size = sizeof(status),
      .flags = IPCZ_NO_FLAGS,
      .num_local_parcels = 0,
      .num_local_bytes = 0,
  };
  for (const Trap& trap : traps_) {
    dispatcher.DeferEvent(trap.handler, trap.context, flags, status);
  }
  traps_.clear();
}

IpczTrapConditionFlags TrapSet::GetSatisfiedConditionsForUpdate(
    const IpczTrapConditions& conditions,
    IpczPortalStatusFlags status_flags,
    ParcelQueue& inbound_parcel_queue,
    UpdateReason reason) {
  IpczTrapConditionFlags event_flags = 0;
  if ((conditions.flags & IPCZ_TRAP_PEER_CLOSED) &&
      (status_flags & IPCZ_PORTAL_STATUS_PEER_CLOSED)) {
    event_flags |= IPCZ_TRAP_PEER_CLOSED;
  }
  if ((conditions.flags & IPCZ_TRAP_DEAD) &&
      (status_flags & IPCZ_PORTAL_STATUS_DEAD)) {
    event_flags |= IPCZ_TRAP_DEAD;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS) &&
      inbound_parcel_queue.GetNumAvailableElements() >
          conditions.min_local_parcels) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
  }
  if ((conditions.flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES) &&
      inbound_parcel_queue.GetTotalAvailableElementSize() >
          conditions.min_local_bytes) {
    event_flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES;
  }
  if ((conditions.flags & IPCZ_TRAP_NEW_LOCAL_PARCEL) &&
      reason == UpdateReason::kNewLocalParcel) {
    event_flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }
  return event_flags;
}

void TrapSet::UpdatePortalStatus(IpczPortalStatusFlags status_flags,
                                 ParcelQueue& inbound_parcel_queue,
                                 UpdateReason reason,
                                 TrapEventDispatcher& dispatcher) {
  for (auto it = traps_.begin(); it != traps_.end();) {
    const Trap& trap = *it;
    IpczTrapConditionFlags flags = GetSatisfiedConditionsForUpdate(
        trap.conditions, status_flags, inbound_parcel_queue, reason);
    if (!flags) {
      ++it;
      continue;
    }

    if (APIContext::IsCurrentThreadWithinAPICall()) {
      flags |= IPCZ_TRAP_WITHIN_API_CALL;
    }

    const IpczPortalStatus status{
        .size = sizeof(status),
        .flags = status_flags,
        .num_local_parcels = inbound_parcel_queue.GetNumAvailableElements(),
        .num_local_bytes = inbound_parcel_queue.GetTotalAvailableElementSize(),
    };
    dispatcher.DeferEvent(trap.handler, trap.context, flags, status);
    it = traps_.erase(it);
  }
}

TrapSet::Trap::Trap(IpczTrapConditions conditions,
                    IpczTrapEventHandler handler,
                    uintptr_t context)
    : conditions(conditions), handler(handler), context(context) {}

TrapSet::Trap::~Trap() = default;

}  // namespace ipcz
