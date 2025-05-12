// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include "base/strings/stringprintf.h"
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
  NOTREACHED();
}

}  // namespace

ScreenInfo::ScreenInfo() = default;
ScreenInfo::ScreenInfo(const ScreenInfo& other) = default;
ScreenInfo::~ScreenInfo() = default;
ScreenInfo& ScreenInfo::operator=(const ScreenInfo& other) = default;

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
