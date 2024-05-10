// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_configuration_params.h"

namespace display {

DisplayConfigurationParams::DisplayConfigurationParams() = default;
DisplayConfigurationParams::DisplayConfigurationParams(
    const DisplayConfigurationParams& other)
    : DisplayConfigurationParams(other.id,
                                 other.origin,
                                 other.mode.get(),
                                 other.enable_vrr) {}
DisplayConfigurationParams::DisplayConfigurationParams(
    DisplayConfigurationParams&& other) = default;

DisplayConfigurationParams::DisplayConfigurationParams(
    int64_t id,
    const gfx::Point& origin,
    const display::DisplayMode* pmode,
    bool enable_vrr)
    : id(id), origin(origin), enable_vrr(enable_vrr) {
  if (pmode)
    mode = pmode->Clone();
}

DisplayConfigurationParams::~DisplayConfigurationParams() = default;

}  // namespace display
