// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <windows.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/win/win_util.h"

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  const int value = GetSystemMetrics(SM_DIGITIZER);
  return (value & NID_READY) &&
         ((value & NID_INTEGRATED_TOUCH) || (value & NID_EXTERNAL_TOUCH));
}

PointerDigitizerType ToPointerDigitizerType(
    POINTER_DEVICE_TYPE pointer_device_type) {
  switch (pointer_device_type) {
    case POINTER_DEVICE_TYPE_INTEGRATED_PEN:
      return PointerDigitizerType::kDirectPen;
    case POINTER_DEVICE_TYPE_EXTERNAL_PEN:
      return PointerDigitizerType::kIndirectPen;
    case POINTER_DEVICE_TYPE_TOUCH:
      return PointerDigitizerType::kTouch;
    case POINTER_DEVICE_TYPE_TOUCH_PAD:
      return PointerDigitizerType::kTouchPad;
    default:
      return PointerDigitizerType::kUnknown;
  }
}

PointerDevice ToPointerDevice(const POINTER_DEVICE_INFO& device) {
  return {.key = device.device,
          .digitizer = ToPointerDigitizerType(device.pointerDeviceType),
          .max_active_contacts = device.maxActiveContacts};
}

}  // namespace

std::pair<int, int> GetAvailablePointerAndHoverTypesImpl() {
  // `IsDeviceUsedAsATablet()` guarantees that the device has a touch screen and
  // has no keyboard connected. On Windows 10 it means that it has verified with
  // `GetSystemMetrics(SM_CONVERTIBLESLATEMODE)`.
  //
  // In this case we don't bother to call `GetSystemMetrics(SM_MOUSEPRESENT)`,
  // since it will rarely return 0 even if no external mouse is connected.
  if (base::win::IsDeviceUsedAsATablet(nullptr)) {
    return {POINTER_TYPE_COARSE, HOVER_TYPE_NONE};
  }

  int pointer_types = IsTouchDevicePresent() ? POINTER_TYPE_COARSE : 0;
  int hover_types = HOVER_TYPE_NONE;
  if (GetSystemMetrics(SM_MOUSEPRESENT) != 0) {
    pointer_types |= POINTER_TYPE_FINE;
    hover_types = HOVER_TYPE_HOVER;
  }
  return {pointer_types ? pointer_types : POINTER_TYPE_NONE, hover_types};
}

TouchScreensAvailability GetTouchScreensAvailability() {
  return IsTouchDevicePresent() ? TouchScreensAvailability::ENABLED
                                : TouchScreensAvailability::NONE;
}

int MaxTouchPoints() {
  return IsTouchDevicePresent() ? GetSystemMetrics(SM_MAXIMUMTOUCHES) : 0;
}

std::optional<PointerDevice> GetPointerDevice(PointerDevice::Key key) {
  POINTER_DEVICE_INFO device;
  return base::win::GetPointerDevice(key, device)
             ? std::make_optional(ToPointerDevice(device))
             : std::nullopt;
}

std::vector<PointerDevice> GetPointerDevices() {
  std::vector<PointerDevice> result;
  if (std::optional<std::vector<POINTER_DEVICE_INFO>> pointer_devices =
          base::win::GetPointerDevices()) {
    result.reserve(pointer_devices->size());
    std::ranges::transform(*pointer_devices, std::back_inserter(result),
                           &ToPointerDevice);
  }
  return result;
}

}  // namespace ui
