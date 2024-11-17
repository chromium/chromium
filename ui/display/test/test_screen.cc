// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/test_screen.h"

#include <vector>

#include "ui/display/display.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/display/display_list.h"
#include "ui/display/display_observer.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace display::test {
namespace {
TestScreen* test_screen = nullptr;
}  // namespace

// static
constexpr gfx::Rect TestScreen::kDefaultScreenBounds;

TestScreen::TestScreen(bool create_display, bool register_screen)
    : register_screen_(register_screen) {
  DCHECK(!test_screen);
  test_screen = this;
  if (register_screen_)
    Screen::SetScreenInstance(this);

  if (!create_display)
    return;
  Display display(1, kDefaultScreenBounds);
  ProcessDisplayChanged(display, /* is_primary = */ true);
}

TestScreen::~TestScreen() {
  DCHECK_EQ(test_screen, this);
  if (register_screen_) {
    DCHECK_EQ(Screen::GetScreen(), this);
    Screen::SetScreenInstance(nullptr);
  }
  test_screen = nullptr;
}

// static
TestScreen* TestScreen::Get() {
  DCHECK_EQ(Screen::GetScreen(), test_screen);
  return test_screen;
}

void TestScreen::set_cursor_screen_point(const gfx::Point& point) {
  cursor_screen_point_ = point;
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

bool TestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return false;
}

gfx::NativeWindow TestScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return gfx::NativeWindow();
}

Display TestScreen::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

void TestScreen::SetCursorScreenPointForTesting(const gfx::Point& point) {
  cursor_screen_point_ = point;
}

#if BUILDFLAG(IS_CHROMEOS)
TabletState TestScreen::GetTabletState() const {
  return state_;
}

void TestScreen::OverrideTabletStateForTesting(TabletState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

  display_list().observers()->Notify(
      &DisplayObserver::OnDisplayTabletStateChanged, state);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace display::test
