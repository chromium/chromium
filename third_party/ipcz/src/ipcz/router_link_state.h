// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_LINK_STATE_H_
#define IPCZ_SRC_IPCZ_ROUTER_LINK_STATE_H_

#include <atomic>
#include <cstdint>
#include <type_traits>

#include "ipcz/ipcz.h"
#include "ipcz/link_side.h"
#include "ipcz/node_name.h"
#include "ipcz/ref_counted_fragment.h"

namespace ipcz {

// Structure shared between both Routers connected by RouterLink. This is used
// to synchronously query and reflect the state of each Router to the other,
// and ultimately to facilitate orderly state changes across the route. This
// may live in shared memory, where it should be managed as a
// RefCountedFragment.
//
// Note that RouterLinkStates are effectively only used by central links.
struct IPCZ_ALIGN(8) RouterLinkState : public RefCountedFragment {
  RouterLinkState();

  // In-place initialization of a new RouterLinkState at `where`.
  static RouterLinkState& Initialize(void* where);

  // Link status which both sides atomically update to coordinate orderly proxy
  // bypass, route closure propagation, and other operations.
  using Status = uint32_t;

  // Status constants follow.

  // This is a fresh central link established to bypass a proxy. The Routers on
  // either side both still have decaying links and therefore cannot yet support
  // another bypass operation.
  static constexpr Status kUnstable = 0;

  // Set if side A or B of this link is stable, respectively, meaning it has no
  // decaying router links. If both bits are set, the link itself is considered
  // to be stable.
  static constexpr Status kSideAStable = 1 << 0;
  static constexpr Status kSideBStable = 1 << 1;
  static constexpr Status kStable = kSideAStable | kSideBStable;

  // When either side attempts to lock this link and fails because ther other
  // side is still unstable, they set their corresponding "waiting" bit instead.
  // Once the other side is stable, this bit informs the other side that they
  // should send a flush notification back to this side to unblock whatever
  // operation was waiting for a stable link.
  static constexpr Status kSideAWaiting = 1 << 2;
  static constexpr Status kSideBWaiting = 1 << 3;

  // Set if this link has been locked by side A or B, respectively. These bits
  // are always mutually exclusive and may only be set once kStable are set. A
  // A link may be locked to initiate bypass of one side, or to propagate route
  // closure from one side.
  static constexpr Status kLockedBySideA = 1 << 4;
  static constexpr Status kLockedBySideB = 1 << 5;

  // Set if the link on either side A or B wishes to be notified when parcels
  // or parcel data are consumed by the other side. In practice these are only
  // set when a router has a trap installed to monitor such conditions, which
  // applications may leverage to e.g. implement a back-pressure mechanism.
  static constexpr Status kSideAMonitoringSideB = 1 << 6;
  static constexpr Status kSideBMonitoringSideA = 1 << 7;

  std::atomic<Status> status{kUnstable};

  // In a situation with three routers A-B-C and a central link between A and
  // B, B will eventually ask C to connect directly to A and bypass B along the
  // route. In order to facilitate this, B will also first stash C's name in
  // this field on the central link between A and B. This is sufficient for A to
  // validate that C is an appropriate source of such a bypass request.
  NodeName allowed_bypass_request_source;

  // These fields approximate the number of parcels and data bytes received and
  // queued for retrieval on each side of this link. Values here are saturated
  // if the actual values would exceed the max uint32_t value.
  std::atomic<uint32_t> num_parcels_on_a{0};
  std::atomic<uint32_t> num_bytes_on_a{0};
  std::atomic<uint32_t> num_parcels_on_b{0};
  std::atomic<uint32_t> num_bytes_on_b{0};

  // More reserved slots, padding out this structure to 64 bytes.
  uint32_t reserved1[6] = {0};

  bool is_locked_by(LinkSide side) const {
    Status s = status.load(std::memory_order_relaxed);
    if (side == LinkSide::kA) {
      return (s & kLockedBySideA) != 0;
    }
    return (s & kLockedBySideB) != 0;
  }

  // Updates the status to reflect that the given `side` is stable, meaning that
  // it's no longer holding onto any decaying links.
  void SetSideStable(LinkSide side);

  // Attempts to lock the state of this link from one side, so that the Router
  // on that side can coordinate its own bypass or propagate its own side's
  // closure. In order for this to succeed, both kStable bits must be set and
  // the link must not already be locked. Returns true iff locked successfully.
  //
  // If the opposite side is still unstable, this sets the waiting bit for
  // `from_side` and returns false.
  //
  // In any other situation, the status is unmodified and this returns false.
  [[nodiscard]] bool TryLock(LinkSide from_side);

  // Unlocks a link previously locked by TryLock().
  void Unlock(LinkSide from_side);

  // If both sides of the link are stable AND `side` was marked as waiting
  // before that happened, this resets the waiting bit and returns true.
  // Otherwise the link's status is unchanged and this returns false.
  //
  // Note that the waiting bit for `side` will have only been set if a prior
  // attempt was made to TryLock() from that side, while the other side was
  // still unstable.
  bool ResetWaitingBit(LinkSide side);

  // Returns a snapshot of the inbound parcel queue state on the given side of
  // this link.
  struct QueueState {
    uint32_t num_parcels;
    uint32_t num_bytes;
  };
  QueueState GetQueueState(LinkSide side) const;

  // Updates the queue state for the given side of this link. Values which
  // exceed 2**32-1 are clamped to that value. Returns true if and only if the
  // opposite side of the link wants to be notified about this update.
  bool UpdateQueueState(LinkSide side, size_t num_parcels, size_t num_bytes);

  // Sets an appropriate bit to indicate whether the router on the given side of
  // this link should notify the opposite side after consuming inbound parcels
  // or parcel data. Returns the previous value of the relevant bit, which may
  // be the same as the old value.
  bool SetSideIsMonitoringPeer(LinkSide side, bool is_monitoring);
};

// The size of this structure is fixed at 64 bytes to ensure that it fits the
// smallest block allocation size supported by NodeLinkMemory.
static_assert(sizeof(RouterLinkState) == 64,
              "RouterLinkState size must be 64 bytes");

// RouterLinkState instances may live in shared memory. Trivial copyability is
// asserted here as a sort of proxy condition to catch changes which might break
// that usage (e.g. introduction of a non-trivial destructor).
static_assert(std::is_trivially_copyable<RouterLinkState>::value,
              "RouterLinkState must be trivially copyable");

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_LINK_STATE_H_
