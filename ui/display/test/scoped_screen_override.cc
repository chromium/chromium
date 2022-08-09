// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/scoped_screen_override.h"

#include "ui/display/screen.h"

namespace display {
namespace test {

ScopedScreenOverride::ScopedScreenOverride(Screen* screen)
    : original_screen_(display::Screen::GetScreen()) {
  display::Screen::SetScreenInstance(screen);
}

ScopedScreenOverride::~ScopedScreenOverride() {
  display::Screen::SetScreenInstance(original_screen_);
}

}  // namespace test
}  // namespace display
