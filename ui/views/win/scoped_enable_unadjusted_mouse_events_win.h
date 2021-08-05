// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_
#define UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/scoped_enable_unadjusted_mouse_events.h"

namespace views {

class HWNDMessageHandler;

// This class handles register and unregister unadjusted mouse events on
// windows. Destroying an instance of this class will unregister unadjusted
// mouse events and stops handling mouse WM_INPUT messages.
class ScopedEnableUnadjustedMouseEventsWin
    : public aura::ScopedEnableUnadjustedMouseEvents {
 public:
  explicit ScopedEnableUnadjustedMouseEventsWin(HWNDMessageHandler* owner);
  ~ScopedEnableUnadjustedMouseEventsWin() override;

  // Register to receive raw mouse input. If success, creates a new
  // ScopedEnableUnadjustedMouseEventsWin instance.
  static std::unique_ptr<ScopedEnableUnadjustedMouseEventsWin> StartMonitor(
      HWNDMessageHandler* owner);

  HWNDMessageHandler* owner_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEnableUnadjustedMouseEventsWin);
};
}  // namespace views

#endif  // UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_
