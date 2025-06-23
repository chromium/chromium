// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_

#include <windows.h>

#include <optional>

#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace win {

namespace internal {
class DisplayInfo;
}  // namespace internal

// A display used by ScreenWin.
// It holds a display and additional parameters used for DPI calculations.
class ScreenWinDisplay final {
 public:
  ScreenWinDisplay();
  explicit ScreenWinDisplay(const internal::DisplayInfo& display_info);
  ScreenWinDisplay(const Display& display,
                   const internal::DisplayInfo& display_info);

  const Display& display() const { return display_; }
  const gfx::Rect& pixel_bounds() const { return pixel_bounds_; }
  const gfx::Vector2dF& pixels_per_inch() const { return pixels_per_inch_; }
  const gfx::Rect& screen_rect() const { return screen_rect_; }
  const gfx::Rect& screen_work_rect() const { return screen_work_rect_; }

  // Returns a cached HMONITOR for the display, if any. Fake and headless
  // displays won't have an HMONITOR.
  const std::optional<HMONITOR>& hmonitor() const { return hmonitor_; }

  Display& modifiable_display() { return display_; }

  // Clear the cached HMONITOR. This should be called whenever WM_DISPLAYCHANGE
  // is received since the handle may no longer be valid.
  void InvalidateHMONITOR();

 private:
  Display display_;
  gfx::Rect pixel_bounds_;
  gfx::Vector2dF pixels_per_inch_;
  // The MONITORINFO::rcMonitor display rectangle in virtual-screen coordinates.
  // Used to derive display::Display bounds, and for window placement logic.
  gfx::Rect screen_rect_;
  // The MONITORINFO::rcWork work area rectangle in virtual-screen coordinates.
  // These are display bounds that exclude system UI, like the Windows taskbar.
  // Used to derive display::Display work areas, and for window placement logic.
  gfx::Rect screen_work_rect_;
  // A cached HMONITOR. This may become invalid on WM_DISPLAYCHANGE.
  std::optional<HMONITOR> hmonitor_;
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
