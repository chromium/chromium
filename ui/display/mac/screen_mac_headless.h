// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_SCREEN_MAC_HEADLESS_H_
#define UI_DISPLAY_MAC_SCREEN_MAC_HEADLESS_H_

#include "ui/display/display.h"
#include "ui/display/headless/headless_screen_manager.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_ui_types.h"

namespace display {

class ScreenMacHeadless : public ScreenBase,
                          public HeadlessScreenManager::Delegate {
 public:
  ScreenMacHeadless();

  ScreenMacHeadless(const ScreenMacHeadless&) = delete;
  ScreenMacHeadless& operator=(const ScreenMacHeadless&) = delete;

  ~ScreenMacHeadless() override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  bool IsHeadless() const override;

  // HeadlessScreenManager::Delegate overrides:
  int64_t AddDisplay(const Display& display) override;
  void RemoveDisplay(int64_t display_id) override;

 private:
  void CreateDisplayList();
};

}  // namespace display

#endif  // UI_DISPLAY_MAC_SCREEN_MAC_HEADLESS_H_
