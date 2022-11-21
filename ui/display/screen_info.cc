// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen_info.h"

namespace display {

ScreenInfo::ScreenInfo() = default;
ScreenInfo::ScreenInfo(const ScreenInfo& other) = default;
ScreenInfo::~ScreenInfo() = default;
ScreenInfo& ScreenInfo::operator=(const ScreenInfo& other) = default;

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return device_scale_factor == other.device_scale_factor &&
         display_color_spaces == other.display_color_spaces &&
         depth == other.depth &&
         depth_per_component == other.depth_per_component &&
         is_monochrome == other.is_monochrome &&
         display_frequency == other.display_frequency && rect == other.rect &&
         available_rect == other.available_rect &&
         size_override == other.size_override &&
         orientation_type == other.orientation_type &&
         orientation_angle == other.orientation_angle &&
         is_extended == other.is_extended && is_primary == other.is_primary &&
         is_internal == other.is_internal && label == other.label &&
         display_id == other.display_id;
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !operator==(other);
}

}  // namespace display
