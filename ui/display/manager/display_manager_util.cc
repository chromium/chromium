// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_util.h"

#include <stddef.h>
#include <algorithm>
#include <array>
#include <cmath>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_snapshot.h"

namespace display {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::NumberToString(state) + ")";
  }
}

std::string RefreshRateThrottleStateToString(RefreshRateThrottleState state) {
  switch (state) {
    case kRefreshRateThrottleEnabled:
      return "THROTTLE_ENABLED";
    case kRefreshRateThrottleDisabled:
      return "THROTTLE_DISABLED";
  }
  NOTREACHED();
  return "unknown refresh rate throttle state (" + base::NumberToString(state) +
         ")";
}

int GetDisplayPower(const std::vector<DisplaySnapshot*>& displays,
                    chromeos::DisplayPowerState state,
                    std::vector<bool>* display_power) {
  int num_on_displays = 0;
  if (display_power)
    display_power->resize(displays.size());

  for (size_t i = 0; i < displays.size(); ++i) {
    bool internal = displays[i]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool on =
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (display_power)
      (*display_power)[i] = on;
    if (on)
      num_on_displays++;
  }
  return num_on_displays;
}

std::vector<const DisplayMode*> GetSeamlessRefreshRateModes(
    const DisplaySnapshot& display,
    const DisplayMode& matching_mode) {
  const float kMinRefreshRate = 60.f;
  const float kEpsilon = 0.01f;

  std::vector<const DisplayMode*> matching_modes;
  for (const std::unique_ptr<const display::DisplayMode>& mode :
       display.modes()) {
    if (matching_mode.is_interlaced() != mode->is_interlaced())
      continue;
    // Filter out modes that are less than 60 Hz. Account for floating point
    // inaccuracies so we don't filter out 59.997 mistakenly.
    if (mode->refresh_rate() < (kMinRefreshRate - kEpsilon))
      continue;

    // Filter out modes whose refresh rate is quicker than the preferred mode.
    if (display.native_mode()->refresh_rate() < mode->refresh_rate())
      continue;

    if (matching_mode.size() == mode->size())
      matching_modes.push_back(mode.get());
  }
  auto refresh_lt = [](const DisplayMode* a, const DisplayMode* b) -> bool {
    return a->refresh_rate() < b->refresh_rate();
  };
  std::sort(matching_modes.begin(), matching_modes.end(), refresh_lt);
  return matching_modes;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

std::string MultipleDisplayStateToString(MultipleDisplayState state) {
  switch (state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      return "INVALID";
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      return "HEADLESS";
    case MULTIPLE_DISPLAY_STATE_SINGLE:
      return "SINGLE";
    case MULTIPLE_DISPLAY_STATE_MULTI_MIRROR:
      return "DUAL_MIRROR";
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED:
      return "MULTI_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

bool GetContentProtectionMethods(DisplayConnectionType type,
                                 uint32_t* protection_mask) {
  switch (type) {
    case DISPLAY_CONNECTION_TYPE_NONE:
    case DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return false;

    case DISPLAY_CONNECTION_TYPE_INTERNAL:
    case DISPLAY_CONNECTION_TYPE_VGA:
    case DISPLAY_CONNECTION_TYPE_NETWORK:
      *protection_mask = CONTENT_PROTECTION_METHOD_NONE;
      return true;

    case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
    case DISPLAY_CONNECTION_TYPE_DVI:
    case DISPLAY_CONNECTION_TYPE_HDMI:
      *protection_mask = CONTENT_PROTECTION_METHOD_HDCP;
      return true;
  }
}

std::vector<float> GetDisplayZoomFactors(const ManagedDisplayMode& mode) {
  // Internal displays have an internal device scale factor greater than 1
  // associated with them. This means that if we use the usual logic, we would
  // end up with a list of zoom levels that the user may not find very useful.
  // Take for example the pixelbook with device scale factor of 2. Based on the
  // usual approach, we would get a zoom range of 90% to 150%. This means:
  //   1. Users will not be able to go to the native resolution which is
  //      achieved at 50% zoom level.
  //   2. Due to the device scale factor, the display already has a low DPI and
  //      users dont want to zoom in, they mostly want to zoom out and add more
  //      pixels to the screen. But we only provide a zoom list of 90% to 150%.
  // This clearly shows we need a different logic to handle internal displays
  // which have lower DPI due to the device scale factor associated with them.
  //
  // OTOH if we look at an external display with a device scale factor of 1 but
  // the same resolution as the pixel book, the DPI would usually be very high
  // and users mostly want to zoom in to reduce the number of pixels on the
  // screen. So having a range of 90% to 130% makes sense.
  // TODO(malaykeshav): Investigate if we can use DPI instead of resolution or
  // device scale factor to decide the list of zoom levels.
  if (mode.device_scale_factor() > 1.f)
    return GetDisplayZoomFactorForDsf(mode.device_scale_factor());

  // There may be cases where the device scale factor is less than 1. This can
  // happen during testing or local linux builds.
  const int effective_width = std::round(
      static_cast<float>(std::max(mode.size().width(), mode.size().height())) /
      mode.device_scale_factor());

  std::size_t index = kZoomListBuckets.size() - 1;
  while (index > 0 && effective_width < kZoomListBuckets[index].first)
    index--;
  DCHECK_GE(effective_width, kZoomListBuckets[index].first);

  const auto& zoom_array = kZoomListBuckets[index].second;
  return std::vector<float>(zoom_array.begin(), zoom_array.end());
}

std::vector<float> GetDisplayZoomFactorForDsf(float dsf) {
  DCHECK(!WithinEpsilon(dsf, 1.f));
  DCHECK_GT(dsf, 1.f);

  for (const auto& bucket : kZoomListBucketsForDsf) {
    if (WithinEpsilon(bucket.first, dsf))
      return std::vector<float>(bucket.second.begin(), bucket.second.end());
  }
  NOTREACHED() << "Received a DSF not on the list: " << dsf;
  return {1.f / dsf, 1.f};
}

}  // namespace display
