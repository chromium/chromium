// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win_display.h"

#include "ui/display/win/display_info.h"

namespace display {
namespace win {
namespace {

Display CreateDisplayFromDisplayInfo(
    const internal::DisplayInfo& display_info) {
  Display display(display_info.id());
  float scale_factor = display_info.device_scale_factor();
  display.set_device_scale_factor(scale_factor);
  display.set_work_area(gfx::ScaleToEnclosingRect(
      display_info.screen_work_rect(), 1.0f / scale_factor));
  display.set_bounds(gfx::ScaleToEnclosingRect(display_info.screen_rect(),
                                               1.0f / scale_factor));
  display.set_rotation(display_info.rotation());
  display.set_display_frequency(display_info.display_frequency());
  display.set_label(display_info.label());
  return display;
}

}  // namespace

ScreenWinDisplay::ScreenWinDisplay() = default;

ScreenWinDisplay::ScreenWinDisplay(const internal::DisplayInfo& display_info)
    : ScreenWinDisplay(CreateDisplayFromDisplayInfo(display_info),
                       display_info) {}

ScreenWinDisplay::ScreenWinDisplay(const Display& display,
                                   const internal::DisplayInfo& display_info)
    : display_(display),
      pixel_bounds_(display_info.screen_rect()),
      pixels_per_inch_(display_info.pixels_per_inch()),
      screen_rect_(display_info.screen_rect()),
      screen_work_rect_(display_info.screen_work_rect()) {}
}  // namespace win
}  // namespace display
