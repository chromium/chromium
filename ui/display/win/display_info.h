// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DISPLAY_INFO_H_
#define UI_DISPLAY_WIN_DISPLAY_INFO_H_

#include <windows.h>

#include <stdint.h>

#include "ui/display/display.h"
#include "ui/display/display_export.h"

namespace display::win::internal {

// Gathers the parameters necessary to create a win::ScreenWinDisplay.
class DISPLAY_EXPORT DisplayInfo final {
 public:
  DisplayInfo(const MONITORINFOEX& monitor_info,
              float device_scale_factor,
              float sdr_white_level,
              Display::Rotation rotation,
              float display_frequency,
              const gfx::Vector2dF& pixels_per_inch,
              DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY output_technology,
              const std::string& label);
  DisplayInfo(const DisplayInfo& other);
  ~DisplayInfo();

  // Derives a display ID using monitor information.
  static int64_t DisplayIdFromMonitorInfo(const MONITORINFOEX& monitor);

  int64_t id() const { return id_; }
  const gfx::Rect& screen_rect() const { return screen_rect_; }
  const gfx::Rect& screen_work_rect() const { return screen_work_rect_; }
  float device_scale_factor() const { return device_scale_factor_; }
  float sdr_white_level() const { return sdr_white_level_; }
  Display::Rotation rotation() const { return rotation_; }
  float display_frequency() const { return display_frequency_; }
  const gfx::Vector2dF& pixels_per_inch() const { return pixels_per_inch_; }
  DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY output_technology() const {
    return output_technology_;
  }
  const std::string& label() const { return label_; }
  const std::wstring& device_name() const { return device_name_; }

  bool operator==(const DisplayInfo& rhs) const;
  bool operator!=(const DisplayInfo& rhs) const { return !(*this == rhs); }

 private:
  int64_t id_;
  // The MONITORINFO::rcMonitor display rectangle in virtual-screen coordinates.
  // Used to derive display::Display bounds, and for window placement logic.
  gfx::Rect screen_rect_;
  // The MONITORINFO::rcWork work area rectangle in virtual-screen coordinates.
  // These are display bounds that exclude system UI, like the Windows taskbar.
  // Used to derive display::Display work areas, and for window placement logic.
  gfx::Rect screen_work_rect_;
  float device_scale_factor_;
  float sdr_white_level_;
  Display::Rotation rotation_;
  float display_frequency_;
  // Pixels per inch of a display. This value will only be set for touch
  // monitors. In non-touch cases, it will be set to Zero.
  gfx::Vector2dF pixels_per_inch_;
  DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY output_technology_;
  std::string label_;
  // The MONITORINFOEX::szDevice device name representing the display.
  std::wstring device_name_;
};

}  // namespace display::win::internal

#endif  // UI_DISPLAY_WIN_DISPLAY_INFO_H_
