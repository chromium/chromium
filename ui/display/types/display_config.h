// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_CONFIG_H_
#define UI_DISPLAY_TYPES_DISPLAY_CONFIG_H_

#include <vector>

#include "ui/display/types/display_geometry.h"
#include "ui/display/types/display_types_export.h"

namespace display {

struct DISPLAY_TYPES_EXPORT DisplayConfig {
  explicit DisplayConfig(float primary_scale);
  DisplayConfig();
  DisplayConfig(DisplayConfig&& other);
  DisplayConfig& operator=(DisplayConfig&& other);
  ~DisplayConfig();

  std::vector<DisplayGeometry> display_geometries;
  float primary_scale = 1.0f;
  float font_scale = 1.0f;

  bool operator==(const DisplayConfig& other) const {
    return display_geometries == other.display_geometries &&
           primary_scale == other.primary_scale &&
           font_scale == other.font_scale;
  }
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_CONFIG_H_
