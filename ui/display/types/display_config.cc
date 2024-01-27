// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_config.h"

namespace display {

DisplayConfig::DisplayConfig(float primary_scale)
    : primary_scale(primary_scale) {}

DisplayConfig::DisplayConfig() = default;

DisplayConfig::DisplayConfig(DisplayConfig&& other) = default;

DisplayConfig& DisplayConfig::operator=(DisplayConfig&& other) = default;

DisplayConfig::~DisplayConfig() = default;

}  // namespace display
