// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_CONFIGURATION_PARAMS_H_
#define UI_DISPLAY_TYPES_DISPLAY_CONFIGURATION_PARAMS_H_

#include <stdint.h>

#include <optional>

#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/point.h"

namespace display {

struct DISPLAY_TYPES_EXPORT DisplayConfigurationParams {
  DisplayConfigurationParams();
  DisplayConfigurationParams(const DisplayConfigurationParams& other);
  DisplayConfigurationParams(DisplayConfigurationParams&& other);
  DisplayConfigurationParams(int64_t id,
                             const gfx::Point& origin,
                             const display::DisplayMode* pmode,
                             bool enable_vrr = false);

  DisplayConfigurationParams& operator=(
      const DisplayConfigurationParams& other);

  ~DisplayConfigurationParams();

  bool operator==(const DisplayConfigurationParams& other) const;

  int64_t id = 0;
  gfx::Point origin = gfx::Point();
  // Set to nullptr to indicate that no mode is set.
  std::unique_ptr<display::DisplayMode> mode = nullptr;
  bool enable_vrr = false;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_CONFIGURATION_PARAMS_H_
