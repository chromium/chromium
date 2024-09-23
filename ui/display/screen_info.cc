// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include "ui/display/screen_info.h"

namespace display {

namespace {

// Returns a debug string for the orientation type.
const char* ToOrientationString(mojom::ScreenOrientation orientation_type) {
  switch (orientation_type) {
    case mojom::ScreenOrientation::kUndefined:
      return "Undefined";
    case mojom::ScreenOrientation::kPortraitPrimary:
      return "PortraitPrimary";
    case mojom::ScreenOrientation::kPortraitSecondary:
      return "PortraitSecondary";
    case mojom::ScreenOrientation::kLandscapePrimary:
      return "LandscapePrimary";
    case mojom::ScreenOrientation::kLandscapeSecondary:
      return "LandscapeSecondary";
  }
  NOTREACHED_IN_MIGRATION();
  return "unknown";
}

}  // namespace

ScreenInfo::ScreenInfo() = default;
ScreenInfo::ScreenInfo(const ScreenInfo& other) = default;
ScreenInfo::~ScreenInfo() = default;
ScreenInfo& ScreenInfo::operator=(const ScreenInfo& other) = default;

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return device_scale_factor == other.device_scale_factor &&
         display_color_spaces == other.display_color_spaces &&
         depth == other.depth &&
         depth_per_component == other.depth_per_component &&
         is_monochrome == other.is_monochrome && rect == other.rect &&
         available_rect == other.available_rect &&
         orientation_type == other.orientation_type &&
         orientation_angle == other.orientation_angle &&
         is_extended == other.is_extended && is_primary == other.is_primary &&
         is_internal == other.is_internal && label == other.label &&
         display_id == other.display_id;
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !operator==(other);
}

std::string ScreenInfo::ToString() const {
  return base::StringPrintf(
      "ScreenInfo[%" PRId64 "] \"%s\" bounds=[%s] avail=[%s] scale=%g %s %s %s",
      display_id, label.c_str(), rect.ToString().c_str(),
      available_rect.ToString().c_str(), device_scale_factor,
      ToOrientationString(orientation_type),
      is_internal ? "internal" : "external",
      is_primary ? "primary" : "secondary");
}

}  // namespace display
