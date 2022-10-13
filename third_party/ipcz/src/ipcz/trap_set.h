// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_TRAP_SET_H_
#define IPCZ_SRC_IPCZ_TRAP_SET_H_

#include <cstdint>

#include "ipcz/ipcz.h"
#include "ipcz/operation_context.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace ipcz {

class TrapEventDispatcher;

// A set of traps installed on a portal.
class TrapSet {
 public:
  // The reason for each status update when something happens that might
  // interest a trap. This is particularly useful for observing edge-triggered
  // conditions.
  enum class UpdateReason {
    // A new trap is being installed and this is an initial state query.
    kInstallTrap,

    // A new inbound parcel has arrived for retrieval.
    kNewLocalParcel,

    // We just discovered that the remote portal is gone.
    kPeerClosed,

    // A previously queued inbound parcel has been fully or partially retrieved
    // by the application.
    kLocalParcelConsumed,

    // A remote peer has changed state in a way that may be interesting to a
    // trap in the set; for example, parcels may have been consumed from the
    // remote queue.
    kRemoteActivity,
  };

  TrapSet();
  TrapSet(const TrapSet&) = delete;
  TrapSet& operator=(const TrapSet&) = delete;

  // NOTE: A TrapSet must be empty before it can be destroyed.
  ~TrapSet();

  bool empty() const { return traps_.empty(); }

  // Indicates whether any installed traps in this set require monitoring of
  // remote queue state.
  bool need_remote_parcels() const {
    return num_traps_monitoring_remote_parcels_ > 0;
  }
  bool need_remote_bytes() const {
    return num_traps_monitoring_remote_bytes_ > 0;
  }
  bool need_remote_state() const {
    return need_remote_parcels() || need_remote_bytes();
  }

  // Returns the set of trap condition flags within `conditions` that would be
  // raised right now if a trap were installed to watch for them, given
  // `current_status` as the status of the portal being watched. If this returns
  // zero (IPCZ_NO_FLAGS), then no watched conditions are satisfied and a
  // corresponding call to Add() would succeed.
  static IpczTrapConditionFlags GetSatisfiedConditions(
      const IpczTrapConditions& conditions,
      const IpczPortalStatus& current_status);

  // Attempts to install a new trap in the set. This effectively implements
  // the ipcz Trap() API. If `conditions` are already met, returns
  // IPCZ_RESULT_FAILED_PRECONDITION and populates `satisfied_condition_flags`
  // and/or `status` if non-null.
  IpczResult Add(const IpczTrapConditions& conditions,
                 IpczTrapEventHandler handler,
                 uintptr_t context,
                 const IpczPortalStatus& current_status,
                 IpczTrapConditionFlags* satisfied_condition_flags,
                 IpczPortalStatus* status);

  // Notifies this TrapSet of a state change on the portal it's interested in.
  // If the state change is interesting to any trap in the set, an appropriate
  // event may be appended to `dispatcher` for imminent dispatch and the trap is
  // removed from the set before returning.
  void UpdatePortalStatus(const OperationContext& context,
                          const IpczPortalStatus& status,
                          UpdateReason reason,
                          TrapEventDispatcher& dispatcher);

  // Immediately removes all traps from the set. Every trap present appends an
  // IPCZ_TRAP_REMOVED event to `dispatcher` before removal.
  void RemoveAll(const OperationContext& context,
                 TrapEventDispatcher& dispatcher);

 private:
  struct Trap {
    Trap(IpczTrapConditions conditions,
         IpczTrapEventHandler handler,
         uintptr_t context);
    ~Trap();

    IpczTrapConditions conditions;
    IpczTrapEventHandler handler;
    uintptr_t context;
  };

  using TrapList = absl::InlinedVector<Trap, 4>;
  TrapList traps_;
  size_t num_traps_monitoring_remote_parcels_ = 0;
  size_t num_traps_monitoring_remote_bytes_ = 0;
  IpczPortalStatus last_known_status_ = {.size = sizeof(last_known_status_)};
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_TRAP_SET_H_
