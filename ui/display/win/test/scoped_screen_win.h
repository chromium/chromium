// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_

#include "base/macros.h"
#include "ui/display/win/screen_win.h"

namespace display {
namespace win {
namespace test {

// ScopedScreenWin construct a instance of ScreenWinDisplay with bounds
// (1920,1080). This will allow unittests to query the details about ScreenWin
// using static methods. ScopedScreenWin needs to be initialized before running
// the code that queries ScreenWin.
class ScopedScreenWin : public ScreenWin {
 public:
  ScopedScreenWin();
  ~ScopedScreenWin() override;

 private:
  Screen* old_screen_ = Screen::SetScreenInstance(this);

  DISALLOW_COPY_AND_ASSIGN(ScopedScreenWin);
};

}  // namespace test
}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_TEST_SCOPED_SCREEN_WIN_H_
