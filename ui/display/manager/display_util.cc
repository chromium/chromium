// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_util.h"

#include <stddef.h>
#include <algorithm>
#include <array>
#include <cmath>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_snapshot.h"

namespace display {
namespace {

// The total number of display zoom factors to enumerate.
constexpr int kNumOfZoomFactors = 9;

// A pair representing the list of zoom values for a given minimum display
// resolution width.
using ZoomListBucket = std::pair<int, std::array<float, kNumOfZoomFactors>>;

// A pair representing the list of zoom values for a given minimum default dsf.
using ZoomListBucketDsf =
    std::pair<float, std::array<float, kNumOfZoomFactors>>;

// For displays with a device scale factor of unity, we use a static list of
// initialized zoom values. For a given resolution width of a display, we can
// find its associated list of zoom values by simply finding the last bucket
// with a width less than the given resolution width.
// Ex. A resolution width of 1024, we will use the bucket with the width of 960.
constexpr std::array<ZoomListBucket, 8> kZoomListBuckets{{
    {0, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
    {720, {0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f}},
    {800, {0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f}},
    {960, {0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f}},
    {1280, {0.90f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f, 1.50f}},
    {1920, {1.f, 1.10f, 1.15f, 1.20f, 1.30f, 1.40f, 1.50f, 1.75f, 2.00f}},
    {3840, {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}},
    {5120, {1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f, 3.00f}},
}};

// Displays with a default device scale factor have a static list of initialized
// zoom values that includes a zoom level to go to the native resolution of the
// display. Ensure that the list of DSFs are in sync with the list of default
// device scale factors in display_change_observer.cc.
constexpr std::array<ZoomListBucketDsf, 7> kZoomListBucketsForDsf{{
    {1.25f, {0.7f, 1.f / 1.25f, 0.85f, 0.9f, 0.95f, 1.f, 1.1f, 1.2f, 1.3f}},
    {1.6f, {1.f / 1.6f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 1.f, 1.15f, 1.3f}},
    {kDsf_1_777,
     {1.f / kDsf_1_777, 0.65f, 0.75f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f}},
    {2.f, {1.f / 2.f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.25f, 1.5f}},
    {kDsf_2_252,
     {1.f / kDsf_2_252, 0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.15f, 1.3f, 1.5f}},
    {2.4f, {1.f / 2.4f, 0.5f, 0.6f, 0.8f, 0.9f, 1.f, 1.2f, 1.35f, 1.5f}},
    {kDsf_2_666,
     {1.f / kDsf_2_666, 0.5f, 0.6f, 0.8f, 0.9f, 1.f, 1.2f, 1.35f, 1.5f}},
}};

}  // namespace

#if defined(OS_CHROMEOS)
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

#endif  // defined(OS_CHROMEOS)

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

void InsertDsfIntoList(std::vector<float>* zoom_values, float dsf) {
  // 1.0 is already in the list of |zoom_values|. We do not need to add it.
  if (WithinEpsilon(dsf, 1.f))
    return;

  if (dsf > 1.f && WithinEpsilon(*(zoom_values->rbegin()), 1.f)) {
    // If the last element of the vector is 1 then |dsf|, which is greater than
    // 1, will simply be inserted after that.
    zoom_values->push_back(dsf);
    zoom_values->erase(zoom_values->begin());
    return;
  } else if (dsf < 1.f && WithinEpsilon(*(zoom_values->begin()), 1.f)) {
    // If the first element in the list is 1 then |dsf|, which is less than 1,
    // will simply be inseted before that.
    zoom_values->insert(zoom_values->begin(), dsf);
    zoom_values->pop_back();
    return;
  }

  // We dont need to add |dsf| to the list if it is already in the list.
  auto it = std::lower_bound(zoom_values->begin(), zoom_values->end(), dsf);
  if (it != zoom_values->end() && WithinEpsilon(*it, dsf))
    return;

  if (it == zoom_values->begin()) {
    DCHECK_LT(dsf, 1.f);
    *(zoom_values->begin()) = dsf;
  } else if (it == zoom_values->end()) {
    DCHECK_GT(dsf, 1.f);
    *(zoom_values->rbegin()) = dsf;
  } else {
    // There can only be 1 entry for 1.f value.
    DCHECK(!(WithinEpsilon(*(it - 1), 1.f) && WithinEpsilon(*it, 1.f)));

    // True if |dsf| is closer to |it| than it is to |it-1|.
    const bool dsf_closer_to_it =
        std::abs(*it - dsf) < std::abs(*(it - 1) - dsf);
    if (WithinEpsilon(*(it - 1), 1.f) ||
        (dsf_closer_to_it && !WithinEpsilon(*it, 1.f))) {
      *it = dsf;
    } else {
      *(it - 1) = dsf;
    }
  }
}

}  // namespace display
