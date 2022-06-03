// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TOPMOST_WINDOW_FINDER_WIN_H_
#define UI_DISPLAY_WIN_TOPMOST_WINDOW_FINDER_WIN_H_

#include "base/win/scoped_gdi_object.h"
#include "ui/display/win/base_window_finder_win.h"
#include "ui/gfx/geometry/point.h"

namespace display {
namespace win {

// Helper class to determine if a particular point of a window is not obscured
// by another window.
class TopMostFinderWin : public BaseWindowFinderWin {
 public:
  // Returns true if |window| is the topmost window at the location
  // |screen_loc|, not including the windows in |ignore|.
  static bool IsTopMostWindowAtPoint(HWND window,
                                     const gfx::Point& screen_loc,
                                     const std::set<HWND>& ignore);

  bool ShouldStopIterating(HWND hwnd) override;

 private:
  TopMostFinderWin(HWND window,
                   const gfx::Point& screen_loc,
                   const std::set<HWND>& ignore);
  TopMostFinderWin(const TopMostFinderWin& finder) = delete;
  TopMostFinderWin& operator=(const TopMostFinderWin& finder) = delete;
  ~TopMostFinderWin() override;

  // The window we're looking for.
  HWND target_;

  // Location of window to find in pixel coordinates.
  gfx::Point screen_loc_;

  // Is target_ the top most window? This is initially false but set to true
  // in ShouldStopIterating if target_ is passed in.
  bool is_top_most_;

  base::win::ScopedRegion tmp_region_;
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_TOPMOST_WINDOW_FINDER_WIN_H_