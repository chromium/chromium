// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_configuration_params.h"

namespace display {

DisplayConfigurationParams::DisplayConfigurationParams() = default;
DisplayConfigurationParams::DisplayConfigurationParams(
    DisplayConfigurationParams& other)
    : id(other.id), origin(other.origin) {
  if (other.mode)
    mode = other.mode->get()->Clone();
}

DisplayConfigurationParams::DisplayConfigurationParams(
    DisplayConfigurationParams&& other)
    : id(other.id), origin(other.origin) {
  if (other.mode)
    mode = other.mode->get()->Clone();
}

DisplayConfigurationParams::DisplayConfigurationParams(
    int64_t id,
    const gfx::Point& origin,
    const display::DisplayMode* pmode)
    : id(id), origin(origin) {
  if (pmode)
    mode = pmode->Clone();
}

DisplayConfigurationParams::~DisplayConfigurationParams() = default;

}  // namespace display
