// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/display_info.h"

#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"

namespace display {
namespace win {

DisplayInfo::DisplayInfo(const MONITORINFOEX& monitor_info,
                         float device_scale_factor,
                         float sdr_white_level,
                         Display::Rotation rotation,
                         int display_frequency,
                         const gfx::Vector2dF& pixels_per_inch)
    : id_(DeviceIdFromDeviceName(monitor_info.szDevice)),
      rotation_(rotation),
      screen_rect_(monitor_info.rcMonitor),
      screen_work_rect_(monitor_info.rcWork),
      device_scale_factor_(device_scale_factor),
      sdr_white_level_(sdr_white_level),
      display_frequency_(display_frequency),
      pixels_per_inch_(pixels_per_inch) {}

DisplayInfo::~DisplayInfo() = default;

// static
int64_t DisplayInfo::DeviceIdFromDeviceName(const wchar_t* device_name) {
  return static_cast<int64_t>(base::Hash(base::WideToUTF8(device_name)));
}

}  // namespace win
}  // namespace display
