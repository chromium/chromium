// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_
#define UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_

#include "base/memory/raw_ptr.h"

namespace display {

class Screen;

namespace test {

// [Deprecated] Do not use this in new code.
//
// This class represents a RAII wrapper for global screen overriding. An object
// of this class restores original display::Screen instance when it goes out of
// scope. Prefer to use it instead of directly call of
// display::Screen::SetScreenInstance().
class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(Screen* screen);

  ScopedScreenOverride(const ScopedScreenOverride&) = delete;
  ScopedScreenOverride& operator=(const ScopedScreenOverride&) = delete;

  ~ScopedScreenOverride();

 private:
  raw_ptr<Screen> original_screen_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_SCOPED_SCREEN_OVERRIDE_H_
