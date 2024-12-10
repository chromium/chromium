// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include "base/check_op.h"
#include "ui/events/devices/input_device_observer_ios.h"

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
  // TODO(crbug.com/379764624): when Apple provides a public alternative,
  // replace the private API with it.
  static InputDeviceObserverIOS* input_device_observer_ios =
      InputDeviceObserverIOS::GetInstance();
  return input_device_observer_ios->GetHasMouseDevice() ? HOVER_TYPE_HOVER
                                                        : HOVER_TYPE_NONE;
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  return POINTER_TYPE_COARSE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  if (available_hover_types & HOVER_TYPE_NONE) {
    return HOVER_TYPE_NONE;
  }
  DCHECK_EQ(available_hover_types, HOVER_TYPE_HOVER);
  return HOVER_TYPE_HOVER;
}

std::optional<PointerDevice> GetPointerDevice(PointerDevice::Key key) {
  return std::nullopt;
}

std::vector<PointerDevice> GetPointerDevices() {
  return {};
}

}  // namespace ui
