// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/scoped_display_for_new_windows.h"

#include "ui/display/screen.h"

namespace display {

ScopedDisplayForNewWindows::ScopedDisplayForNewWindows(int64_t new_display) {
  Screen::GetScreen()->SetScopedDisplayForNewWindows(new_display);
}

ScopedDisplayForNewWindows::ScopedDisplayForNewWindows(gfx::NativeView view)
    : ScopedDisplayForNewWindows(
          Screen::GetScreen()->GetDisplayNearestView(view).id()) {}

ScopedDisplayForNewWindows::~ScopedDisplayForNewWindows() {
  Screen::GetScreen()->SetScopedDisplayForNewWindows(
      display::kInvalidDisplayId);
}

}  // namespace display
