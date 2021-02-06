// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_
#define UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_

#include "base/macros.h"

namespace display {

class Screen;

namespace test {

// This class represents a RAII wrapper for global screen overriding. An object
// of this class restores original display::Screen instance when it goes out of
// scope. Prefer to use it instead of directly call of
// display::Screen::SetScreenInstance().
class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(Screen* screen);
  ~ScopedScreenOverride();

 private:
  Screen* original_screen_;
  DISALLOW_COPY_AND_ASSIGN(ScopedScreenOverride);
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_
