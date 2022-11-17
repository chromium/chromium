// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router_link_state.h"

#include <cstring>
#include <limits>

#include "ipcz/link_side.h"

namespace ipcz {

namespace {

template <typename T, typename U>
void StoreSaturated(std::atomic<T>& dest, U value) {
  if (value < std::numeric_limits<T>::max()) {
    dest.store(value, std::memory_order_relaxed);
  } else {
    dest.store(std::numeric_limits<T>::max(), std::memory_order_relaxed);
  }
}

template <typename T>
T& SelectBySide(LinkSide side, T& for_a, T& for_b) {
  if (side.is_side_a()) {
    return for_a;
  }
  return for_b;
}

}  // namespace

RouterLinkState::RouterLinkState() = default;

// static
RouterLinkState& RouterLinkState::Initialize(void* where) {
  auto& state = *static_cast<RouterLinkState*>(where);
  new (&state) RouterLinkState();
  std::atomic_thread_fence(std::memory_order_release);
  return state;
}

void RouterLinkState::SetSideStable(LinkSide side) {
  const Status kThisSideStable =
      side == LinkSide::kA ? kSideAStable : kSideBStable;

  Status expected = kUnstable;
  while (!status.compare_exchange_weak(expected, expected | kThisSideStable,
                                       std::memory_order_relaxed) &&
         (expected & kThisSideStable) == 0) {
  }
}

bool RouterLinkState::TryLock(LinkSide from_side) {
  const Status kThisSideStable =
      from_side == LinkSide::kA ? kSideAStable : kSideBStable;
  const Status kOtherSideStable =
      from_side == LinkSide::kA ? kSideBStable : kSideAStable;
  const Status kLockedByThisSide =
      from_side == LinkSide::kA ? kLockedBySideA : kLockedBySideB;
  const Status kLockedByEitherSide = kLockedBySideA | kLockedBySideB;
  const Status kThisSideWaiting =
      from_side == LinkSide::kA ? kSideAWaiting : kSideBWaiting;

  Status expected = kStable;
  Status desired_bit = kLockedByThisSide;
  while (!status.compare_exchange_weak(expected, expected | desired_bit,
                                       std::memory_order_relaxed)) {
    if ((expected & kLockedByEitherSide) != 0 ||
        (expected & kThisSideStable) == 0) {
      return false;
    }

    if (desired_bit == kLockedByThisSide &&
        (expected & kOtherSideStable) == 0) {
      // If we were trying to lock but the other side isn't stable, try to set
      // our waiting bit instead.
      desired_bit = kThisSideWaiting;
    } else if (desired_bit == kThisSideWaiting &&
               (expected & kStable) == kStable) {
      // Otherwise if we were trying to set our waiting bit and the other side
      // is now stable, go back to trying to lock the link.
      desired_bit = kLockedByThisSide;
    }
  }

  return desired_bit == kLockedByThisSide;
}

void RouterLinkState::Unlock(LinkSide from_side) {
  const Status kLockedByThisSide =
      from_side == LinkSide::kA ? kLockedBySideA : kLockedBySideB;
  Status expected = kStable | kLockedByThisSide;
  Status desired = kStable;
  while (!status.compare_exchange_weak(expected, desired,
                                       std::memory_order_relaxed) &&
         (expected & kLockedByThisSide) != 0) {
    desired = expected & ~kLockedByThisSide;
  }
}

bool RouterLinkState::ResetWaitingBit(LinkSide side) {
  const Status kThisSideWaiting =
      side == LinkSide::kA ? kSideAWaiting : kSideBWaiting;
  const Status kLockedByEitherSide = kLockedBySideA | kLockedBySideB;
  Status expected = kStable | kThisSideWaiting;
  Status desired = kStable;
  while (!status.compare_exchange_weak(expected, desired,
                                       std::memory_order_relaxed)) {
    if ((expected & kStable) != kStable || (expected & kThisSideWaiting) == 0 ||
        (expected & kLockedByEitherSide) != 0) {
      // If the link isn't stable yet, or `side` wasn't waiting on it, or the
      // link is already locked, there's no point changing the status here.
      return false;
    }

    // At this point we know the link is stable, the identified side is waiting,
    // and the link is not locked. Regardless of what other bits are set, mask
    // off the waiting bit and try to update our status again.
    desired = expected & ~kThisSideWaiting;
  }

  return true;
}

}  // namespace ipcz
