// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

namespace ui {

// Platforms supporting touch link in an alternate implementation of this
// method.
TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::NONE;
}

int MaxTouchPoints() {
  return 0;
}

std::pair<int, int> GetAvailablePointerAndHoverTypes() {
  // Assume a non-touch-device with a mouse
  return std::make_pair(POINTER_TYPE_FINE, HOVER_TYPE_HOVER);
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  // Assume a non-touch-device with a mouse
  return POINTER_TYPE_FINE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  // Assume a non-touch-device with a mouse
  return HOVER_TYPE_HOVER;
}

std::optional<PointerDevice> GetPointerDevice(PointerDevice::Key key) {
  return std::nullopt;
}

std::vector<PointerDevice> GetPointerDevices() {
  return {};
}

}  // namespace ui
