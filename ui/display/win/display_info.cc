// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/display_info.h"

#include <string.h>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/win/display_config_helper.h"

namespace display::win::internal {

namespace {

// Return a string view from a fixed-length array representing a string, up
// until the first nul terminator, if any.
template <size_t N>
std::wstring_view FixedArrayToStringView(
    const std::wstring_view::value_type (&str)[N]) {
  return std::wstring_view(str, ::wcsnlen(str, N));
}

}  // namespace

DisplayInfo::DisplayInfo(
    const MONITORINFOEX& monitor_info,
    float device_scale_factor,
    float sdr_white_level,
    Display::Rotation rotation,
    float display_frequency,
    const gfx::Vector2dF& pixels_per_inch,
    DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY output_technology,
    const std::string& label)
    : id_(DisplayIdFromMonitorInfo(monitor_info)),
      screen_rect_(monitor_info.rcMonitor),
      screen_work_rect_(monitor_info.rcWork),
      device_scale_factor_(device_scale_factor),
      sdr_white_level_(sdr_white_level),
      rotation_(rotation),
      display_frequency_(display_frequency),
      pixels_per_inch_(pixels_per_inch),
      output_technology_(output_technology),
      label_(label),
      device_name_(FixedArrayToStringView(monitor_info.szDevice)) {}

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
  device_name_ = other.device_name_;
}

DisplayInfo::~DisplayInfo() = default;

// static
int64_t DisplayInfo::DisplayIdFromMonitorInfo(const MONITORINFOEX& monitor) {
  // Derive a display ID from the adapter ID and per-adapter monitor ID.
  // This seems to be broadly available, unique for each monitor of the device,
  // and stable across display configuration changes, but not device restarts.
  std::optional<DISPLAYCONFIG_PATH_INFO> config_path =
      GetDisplayConfigPathInfo(monitor);
  // Record if DISPLAYCONFIG_PATH_INFO is available or not.
  base::UmaHistogramBoolean("Windows.LegacyDisplayIdAlgorithm",
                            !config_path.has_value());
  if (config_path.has_value()) {
    return static_cast<int64_t>(base::PersistentHash(base::StringPrintf(
        "%lu/%li/%u", config_path->targetInfo.adapterId.LowPart,
        config_path->targetInfo.adapterId.HighPart,
        config_path->targetInfo.id)));
  }
  // MONITORINFOEX::szDevice is a plausible backup with some notable drawbacks.
  // This value (e.g. "\\.\DISPLAY1") may change when adding/removing displays,
  // and even be reassigned between physical monitors during those changes,
  // which can cause subtle unexpected behavior.
  return static_cast<int64_t>(base::PersistentHash(
      base::WideToUTF8(FixedArrayToStringView(monitor.szDevice))));
}

bool DisplayInfo::operator==(const DisplayInfo& rhs) const = default;

}  // namespace display::win::internal
