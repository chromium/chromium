// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_BASE_WINDOW_FINDER_WIN_H_
#define UI_DISPLAY_WIN_BASE_WINDOW_FINDER_WIN_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/win/windows_types.h"

namespace display {
namespace win {

// Base class used to locate a window. This is intended to be used with the
// various win32 functions that iterate over windows.
//
// A subclass need only override ShouldStopIterating to determine when
// iteration should stop.
class BaseWindowFinderWin {
 public:
  // Creates a BaseWindowFinderWin with the specified set of HWNDs to ignore.
  explicit BaseWindowFinderWin(const std::set<HWND>& ignore);
  BaseWindowFinderWin(const BaseWindowFinderWin& finder) = delete;
  BaseWindowFinderWin& operator=(const BaseWindowFinderWin& finder) = delete;
  virtual ~BaseWindowFinderWin();

 protected:
  static BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam);

  LPARAM as_lparam() {
    // Cast must match that in WindowCallbackProc().
    return reinterpret_cast<LPARAM>(static_cast<BaseWindowFinderWin*>(this));
  }

  // Returns true if iteration should stop, false if iteration should continue.
  virtual bool ShouldStopIterating(HWND window) = 0;

 private:
  const raw_ref<const std::set<HWND>> ignore_;
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_BASE_WINDOW_FINDER_WIN_H_