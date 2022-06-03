// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

PlatformWindowInitProperties::PlatformWindowInitProperties() = default;

PlatformWindowInitProperties::PlatformWindowInitProperties(
    const gfx::Rect& bounds,
    bool to_enable_compositing_based_throttling)
    : bounds(bounds),
      enable_compositing_based_throttling(
          to_enable_compositing_based_throttling) {}

PlatformWindowInitProperties::PlatformWindowInitProperties(
    PlatformWindowInitProperties&& props) = default;

PlatformWindowInitProperties::~PlatformWindowInitProperties() = default;

}  // namespace ui
