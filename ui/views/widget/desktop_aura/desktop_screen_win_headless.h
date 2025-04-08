// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_HEADLESS_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_HEADLESS_H_

#include <set>

#include "ui/display/win/screen_win_headless.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopScreenWinHeadless
    : public display::win::ScreenWinHeadless {
 public:
  DesktopScreenWinHeadless();
  DesktopScreenWinHeadless(const DesktopScreenWinHeadless&) = delete;
  DesktopScreenWinHeadless& operator=(const DesktopScreenWinHeadless&) = delete;
  ~DesktopScreenWinHeadless() override;

 private:
  // display::win::ScreenWin:
  HWND GetHWNDFromNativeWindow(gfx::NativeWindow window) const override;
  gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const override;
  bool IsNativeWindowOccluded(gfx::NativeWindow window) const override;
  std::optional<bool> IsWindowOnCurrentVirtualDesktop(
      gfx::NativeWindow window) const override;

  // display::win::ScreenWinHeadless:
  gfx::Rect GetNativeWindowBoundsInScreen(
      gfx::NativeWindow window) const override;
  gfx::Rect GetHeadlessWindowBounds(
      gfx::AcceleratedWidget window) const override;
  gfx::NativeWindow GetNativeWindowAtScreenPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) const override;
  gfx::NativeWindow GetRootWindow(gfx::NativeWindow window) const override;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_WIN_HEADLESS_H_
