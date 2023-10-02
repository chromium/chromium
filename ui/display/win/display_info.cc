// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/display_info.h"

#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"

namespace display::win::internal {

DisplayInfo::DisplayInfo(
    const MONITORINFOEX& monitor_info,
    float device_scale_factor,
    float sdr_white_level,
    Display::Rotation rotation,
    int display_frequency,
    const gfx::Vector2dF& pixels_per_inch,
    DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY output_technology,
    const std::string& label)
    : id_(DeviceIdFromDeviceName(monitor_info.szDevice)),
      screen_rect_(monitor_info.rcMonitor),
      screen_work_rect_(monitor_info.rcWork),
      device_scale_factor_(device_scale_factor),
      sdr_white_level_(sdr_white_level),
      rotation_(rotation),
      display_frequency_(display_frequency),
      pixels_per_inch_(pixels_per_inch),
      output_technology_(output_technology),
      label_(label) {}

DisplayInfo::DisplayInfo(const DisplayInfo& other) {
  id_ = other.id_;
  screen_rect_ = other.screen_rect_;
  screen_work_rect_ = other.screen_work_rect_;
  device_scale_factor_ = other.device_scale_factor_;
  sdr_white_level_ = other.sdr_white_level_;
  rotation_ = other.rotation_;
  display_frequency_ = other.display_frequency_;
  pixels_per_inch_ = other.pixels_per_inch_;
  output_technology_ = other.output_technology_;
  label_ = other.label_;
}

DisplayInfo::~DisplayInfo() = default;

// static
int64_t DisplayInfo::DeviceIdFromDeviceName(const wchar_t* device_name) {
  return static_cast<int64_t>(
      base::PersistentHash(base::WideToUTF8(device_name)));
}

bool DisplayInfo::operator==(const DisplayInfo& rhs) const {
  return id_ == rhs.id_ && screen_rect_ == rhs.screen_rect_ &&
         screen_work_rect_ == rhs.screen_work_rect_ &&
         device_scale_factor_ == rhs.device_scale_factor_ &&
         sdr_white_level_ == rhs.sdr_white_level_ &&
         rotation_ == rhs.rotation_ &&
         display_frequency_ == rhs.display_frequency_ &&
         pixels_per_inch_ == rhs.pixels_per_inch_ &&
         output_technology_ == rhs.output_technology_ && label_ == rhs.label_;
}

}  // namespace display::win::internal
