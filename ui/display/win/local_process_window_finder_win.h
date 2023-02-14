// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_LOCAL_PROCESS_WINDOW_FINDER_WIN_H_
#define UI_DISPLAY_WIN_LOCAL_PROCESS_WINDOW_FINDER_WIN_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "ui/display/win/base_window_finder_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace display::win {

class ScreenWin;

// Helper class to determine if a particular point contains a window from our
// process.
class LocalProcessWindowFinder : public BaseWindowFinderWin {
 public:
  // Returns the hwnd from our process at screen_loc that is not obscured by
  // another window. Returns NULL otherwise.
  static gfx::NativeWindow GetProcessWindowAtPoint(const gfx::Point& screen_loc,
                                                   const std::set<HWND>& ignore,
                                                   ScreenWin* screen_win);

 protected:
  bool ShouldStopIterating(HWND hwnd) override;

 private:
  LocalProcessWindowFinder(const gfx::Point& screen_loc,
                           ScreenWin* screen_win,
                           const std::set<HWND>& ignore);
  LocalProcessWindowFinder(const LocalProcessWindowFinder& finder) = delete;
  LocalProcessWindowFinder& operator=(const LocalProcessWindowFinder& finder) =
      delete;
  ~LocalProcessWindowFinder() override;

  // Position of the mouse in pixel coordinates.
  gfx::Point screen_loc_;

  // The resulting window. This is set to true in ShouldStopIterating if an
  // appropriate window is found.
  HWND result_ = nullptr;

  // ScreenWin we're looking on. Used to access WindowTreeHost, which
  // ui/display code can't access directly.
  raw_ptr<ScreenWin> screen_win_;
};

}  // namespace display::win

#endif  // UI_DISPLAY_WIN_LOCAL_PROCESS_WINDOW_FINDER_WIN_H_
