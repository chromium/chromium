// Copyright 2017 The Chromium Authors
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

bool Event::Wait(webrtc::TimeDelta give_up_after) {
  if (give_up_after.IsPlusInfinity()) {
    event_.Wait();
    return true;
  }
  return event_.TimedWait(base::Microseconds(give_up_after.us()));
}

}  // namespace rtc
