// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <utility>

#include "ui/events/devices/input_device_observer_ios.h"

namespace ui {

std::pair<int, int> GetAvailablePointerAndHoverTypesImpl() {
  static InputDeviceObserverIOS* input_device_observer_ios =
      InputDeviceObserverIOS::GetInstance();
  return {POINTER_TYPE_COARSE, input_device_observer_ios->GetHasMouseDevice()
                                   ? HOVER_TYPE_HOVER
                                   : HOVER_TYPE_NONE};
}

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return 5;
}

}  // namespace ui
