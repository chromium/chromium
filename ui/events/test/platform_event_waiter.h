// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_PLATFORM_EVENT_WAITER_H_
#define UI_EVENTS_TEST_PLATFORM_EVENT_WAITER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "ui/events/platform/platform_event_observer.h"

namespace ui {

class PlatformEventWaiter : public PlatformEventObserver {
 public:
  using PlatformEventMatcher =
      base::RepeatingCallback<bool(const PlatformEvent&)>;

  static PlatformEventWaiter* Create(base::OnceClosure success_callback,
                                     PlatformEventMatcher event_matcher);

 private:
  PlatformEventWaiter(base::OnceClosure success_callback,
                      PlatformEventMatcher event_matcher);
  ~PlatformEventWaiter() override;

  // PlatformEventObserver:
  void WillProcessEvent(const PlatformEvent& event) override;
  void DidProcessEvent(const PlatformEvent& event) override;

  base::OnceClosure success_callback_;
  PlatformEventMatcher event_matcher_;

  DISALLOW_COPY_AND_ASSIGN(PlatformEventWaiter);
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_PLATFORM_EVENT_WAITER_H_
