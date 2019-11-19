// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"

namespace rtc {

// Overrides WebRTC's internal event implementation to use Chromium's.
class RTC_EXPORT Event {
 public:
  static const int kForever = -1;

  Event();
  Event(bool manual_reset, bool initially_signaled);
  ~Event();

  void Set();
  void Reset();

  // Wait for the event to become signaled, for the specified number of
  // milliseconds.  To wait indefinetly, pass kForever.
  bool Wait(int give_up_after_ms);
  bool Wait(int give_up_after_ms, int /*warn_after_ms*/) {
    return Wait(give_up_after_ms);
  }

 private:
  base::WaitableEvent event_;
  DISALLOW_COPY_AND_ASSIGN(Event);
};

// Pull ScopedAllowBaseSyncPrimitives(ForTesting) into the rtc namespace.
// Managing what types in WebRTC are allowed to use
// ScopedAllowBaseSyncPrimitives, is done via thread_restrictions.h.
using ScopedAllowBaseSyncPrimitives = base::ScopedAllowBaseSyncPrimitives;
using ScopedAllowBaseSyncPrimitivesForTesting =
    base::ScopedAllowBaseSyncPrimitivesForTesting;

}  // namespace rtc

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_EVENT_H_
