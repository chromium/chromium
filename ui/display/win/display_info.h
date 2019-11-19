// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DISPLAY_INFO_H_
#define UI_DISPLAY_WIN_DISPLAY_INFO_H_

#include <windows.h>
#include <stdint.h>

#include "ui/display/display.h"
#include "ui/display/display_export.h"

namespace display {
namespace win {

// Gathers the parameters necessary to create a win::ScreenWinDisplay.
class DISPLAY_EXPORT DisplayInfo final {
 public:
  DisplayInfo(const MONITORINFOEX& monitor_info,
              float device_scale_factor,
              float sdr_white_level,
              Display::Rotation rotation,
              int display_frequency,
              const gfx::Vector2dF& pixels_per_inch);
  ~DisplayInfo();

  static int64_t DeviceIdFromDeviceName(const wchar_t* device_name);

  int64_t id() const { return id_; }
  Display::Rotation rotation() const { return rotation_; }
  const gfx::Rect& screen_rect() const { return screen_rect_; }
  const gfx::Rect& screen_work_rect() const { return screen_work_rect_; }
  float device_scale_factor() const { return device_scale_factor_; }
  float sdr_white_level() const { return sdr_white_level_; }
  int display_frequency() const { return display_frequency_; }
  const gfx::Vector2dF& pixels_per_inch() const { return pixels_per_inch_; }

 private:
  int64_t id_;
  Display::Rotation rotation_;
  gfx::Rect screen_rect_;
  gfx::Rect screen_work_rect_;
  float device_scale_factor_;
  float sdr_white_level_;
  int display_frequency_;
  // Pixels per inch of a display. This value will only be set for touch
  // monitors. In non-touch cases, it will be set to Zero.
  gfx::Vector2dF pixels_per_inch_;
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_DISPLAY_INFO_H_
