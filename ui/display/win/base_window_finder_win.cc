// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/base_window_finder_win.h"

#include <objbase.h>

namespace display {
namespace win {

// Creates a BaseWindowFinderWin with the specified set of HWNDs to ignore.
BaseWindowFinderWin::BaseWindowFinderWin(const std::set<HWND>& ignore)
    : ignore_(ignore) {}

BaseWindowFinderWin::~BaseWindowFinderWin() = default;

// static
BOOL CALLBACK BaseWindowFinderWin::WindowCallbackProc(HWND hwnd,
                                                      LPARAM lParam) {
  // Cast must match that in as_lparam().
  BaseWindowFinderWin* finder = reinterpret_cast<BaseWindowFinderWin*>(lParam);
  if (finder->ignore_.find(hwnd) != finder->ignore_.end())
    return TRUE;

  return finder->ShouldStopIterating(hwnd) ? FALSE : TRUE;
}

}  // namespace win
}  // namespace display