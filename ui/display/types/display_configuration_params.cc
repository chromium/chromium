// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_configuration_params.h"

namespace display {

DisplayConfigurationParams::DisplayConfigurationParams() = default;
DisplayConfigurationParams::DisplayConfigurationParams(
    const DisplayConfigurationParams& other)
    : id(other.id), origin(other.origin), enable_vrr(other.enable_vrr) {
  if (other.mode)
    mode = other.mode->get()->Clone();
}

DisplayConfigurationParams::DisplayConfigurationParams(
    DisplayConfigurationParams&& other)
    : id(std::move(other.id)),
      origin(std::move(other.origin)),
      mode(std::move(other.mode)),
      enable_vrr(other.enable_vrr) {}

DisplayConfigurationParams::DisplayConfigurationParams(
    int64_t id,
    const gfx::Point& origin,
    const display::DisplayMode* pmode)
    : DisplayConfigurationParams(id, origin, pmode, /*enable_vrr=*/false) {}

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
