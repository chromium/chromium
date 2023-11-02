// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_
#define UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_

#include <memory>

#include "base/memory/raw_ptr.h"
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

  ScopedEnableUnadjustedMouseEventsWin(
      const ScopedEnableUnadjustedMouseEventsWin&) = delete;
  ScopedEnableUnadjustedMouseEventsWin& operator=(
      const ScopedEnableUnadjustedMouseEventsWin&) = delete;

  ~ScopedEnableUnadjustedMouseEventsWin() override;

  // Register to receive raw mouse input. If success, creates a new
  // ScopedEnableUnadjustedMouseEventsWin instance.
  static std::unique_ptr<ScopedEnableUnadjustedMouseEventsWin> StartMonitor(
      HWNDMessageHandler* owner);

  raw_ptr<HWNDMessageHandler> owner_;
};
}  // namespace views

#endif  // UI_VIEWS_WIN_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_WIN_H_
