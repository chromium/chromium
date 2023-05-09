// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

float GetScaleFactorForNativeView(gfx::NativeView view) {
  // A number of unit tests do not setup the screen.
  if (!display::Screen::GetScreen())
    return 1.0f;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);

  // GetDisplayNearestView() may return null Display if the |view| is not shown
  // on the screen and there is no primary display. In that case use scale
  // factor 1.0.
  if (!display.is_valid())
    return 1.0f;

  return display.device_scale_factor();
}

}  // namespace ui
