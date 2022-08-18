// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"

namespace rtc {

// Overrides WebRTC's internal event implementation to use Chromium's.
class RTC_EXPORT Event {
 public:
  // TODO(bugs.webrtc.org/14366): Consider removing this redundant alias.
  static constexpr webrtc::TimeDelta kForever =
      webrtc::TimeDelta::PlusInfinity();

  Event();
  Event(bool manual_reset, bool initially_signaled);

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  ~Event();

  void Set();
  void Reset();

  // Wait for the event to become signaled, for the specified duration. To wait
  // indefinitely, pass kForever.
  bool Wait(webrtc::TimeDelta give_up_after);
  // TODO(bugs.webrtc.org/13756): Remove after millisec-based Wait is removed.
  bool Wait(int give_up_after_ms) {
    // SocketServer users can get here with SocketServer::kForever which is -1.
    // Mirror the definition here to avoid dependence.
    constexpr int kForeverMs = -1;
    return Wait(give_up_after_ms == kForeverMs
                    ? kForever
                    : webrtc::TimeDelta::Millis(give_up_after_ms));
  }
  // TODO(bugs.webrtc.org/13756): De-template this after millisec-based Wait is
  // removed.
  template <class T, class U>
  bool Wait(T give_up_after, U /*warn_after*/) {
    return Wait(give_up_after);
  }

 private:
  base::WaitableEvent event_;
};

// Pull ScopedAllowBaseSyncPrimitives(ForTesting) into the rtc namespace.
// Managing what types in WebRTC are allowed to use
// ScopedAllowBaseSyncPrimitives, is done via thread_restrictions.h.
using ScopedAllowBaseSyncPrimitives = base::ScopedAllowBaseSyncPrimitives;
using ScopedAllowBaseSyncPrimitivesForTesting =
    base::ScopedAllowBaseSyncPrimitivesForTesting;

}  // namespace rtc

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_
