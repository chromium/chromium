// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

namespace ui {

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return 5;
}

int GetAvailablePointerTypes() {
  return POINTER_TYPE_COARSE;
}

int GetAvailableHoverTypes() {
  return HOVER_TYPE_HOVER;
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  return POINTER_TYPE_COARSE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  return HOVER_TYPE_NONE;
}

std::optional<PointerDevice> GetPointerDevice(PointerDevice::Key key) {
  return std::nullopt;
}

std::vector<PointerDevice> GetPointerDevices() {
  return {};
}

}  // namespace ui
