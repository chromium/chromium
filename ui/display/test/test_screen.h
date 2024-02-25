// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_TEST_SCREEN_H_
#define UI_DISPLAY_TEST_TEST_SCREEN_H_

#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/display/tablet_state.h"

namespace display::test {

// A dummy implementation of Screen that contains a single
// Display only. The contained Display can be accessed and modified via
// TestScreen::display().
//
// NOTE: Adding and removing DisplayObserver's are no-ops and observers
// will NOT be notified ever.
class TestScreen : public ScreenBase {
 public:
  static constexpr gfx::Rect kDefaultScreenBounds = gfx::Rect(0, 0, 800, 600);

  static TestScreen* Get();

  // TODO(weili): Split this into a protected no-argument constructor for
  // subclass uses and the public one with gfx::Size argument.
  explicit TestScreen(bool create_display = true, bool register_screen = false);
  TestScreen(const TestScreen&) = delete;
  TestScreen& operator=(const TestScreen&) = delete;
  ~TestScreen() override;

  // Sets the fake cursor location for the TestScreen.
  void set_cursor_screen_point(const gfx::Point& point);

  // ScreenBase:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  void SetCursorScreenPointForTesting(const gfx::Point& point) override;
#if BUILDFLAG(IS_CHROMEOS)
  TabletState GetTabletState() const override;
  void OverrideTabletStateForTesting(TabletState state) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  gfx::Point cursor_screen_point_;
  bool register_screen_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  TabletState state_ = TabletState::kInClamshellMode;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace display::test

#endif  // UI_DISPLAY_TEST_TEST_SCREEN_H_
