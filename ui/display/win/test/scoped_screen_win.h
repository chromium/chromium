// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_

#include "base/memory/raw_ptr.h"
#include "ui/display/win/screen_win.h"

namespace display {
namespace win {
namespace test {

// [Deprecated]
// TODO(crbug.com/40222482): The initialization code of this class should be
// moved to the test that depends on it.
//
// ScopedScreenWin construct a instance of ScreenWinDisplay with bounds
// (1920,1080). This will allow unittests to query the details about ScreenWin
// using static methods. ScopedScreenWin needs to be initialized before running
// the code that queries ScreenWin.
class ScopedScreenWin : public ScreenWin {
 public:
  ScopedScreenWin();

  ScopedScreenWin(const ScopedScreenWin&) = delete;
  ScopedScreenWin& operator=(const ScopedScreenWin&) = delete;

  ~ScopedScreenWin() override = default;
};

}  // namespace test
}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_
