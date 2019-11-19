// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/rtc_base/event.h"

#include "base/time/time.h"

namespace rtc {

using base::WaitableEvent;

Event::Event() : Event(false, false) {}

Event::Event(bool manual_reset, bool initially_signaled)
    : event_(manual_reset ? WaitableEvent::ResetPolicy::MANUAL
                          : WaitableEvent::ResetPolicy::AUTOMATIC,
             initially_signaled ? WaitableEvent::InitialState::SIGNALED
                                : WaitableEvent::InitialState::NOT_SIGNALED) {}

Event::~Event() {}

void Event::Set() {
  event_.Signal();
}

void Event::Reset() {
  event_.Reset();
}

bool Event::Wait(int give_up_after_ms) {
  if (give_up_after_ms == kForever) {
    event_.Wait();
    return true;
  }

  return event_.TimedWait(base::TimeDelta::FromMilliseconds(give_up_after_ms));
}

}  // namespace rtc
