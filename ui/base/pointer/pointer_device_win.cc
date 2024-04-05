// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/win/win_util.h"
#include "ui/base/win/hidden_window.h"

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  int value = GetSystemMetrics(SM_DIGITIZER);
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

// The following method logic is as follow :
// - On versions prior to Windows 8 it will always return POINTER_TYPE_FINE
// and/or POINTER_TYPE_COARSE (if the device has a touch screen).
// - If the device is a detachable/convertible device and the keyboard/trackpad
// is detached/flipped it will always return POINTER_TYPE_COARSE.
// It does not cover the case where an external mouse/keyboard is connected
// while the device is used as a tablet. This is because Windows doesn't provide
// us a reliable way to detect keyboard/mouse presence with
// GetSystemMetrics(SM_MOUSEPRESENT).
// - If the device doesn't have a touch screen it will return POINTER_TYPE_FINE.
// In the rare cases (this is Microsoft documentation) where
// GetSystemMetrics(SM_MOUSEPRESENT) returns 0 we will return POINTER_TYPE_NONE.
// - If the device has a touch screen the available pointer devices are
// POINTER_TYPE_FINE and POINTER_TYPE_COARSE.
int GetAvailablePointerTypes() {
  // IsTabletDevice guarantees us that :
  // - The device has a touch screen.
  // - It is used as a tablet which means that it has no keyboard connected.
  // On Windows 10 it means that it is verifying with ConvertibleSlateMode.
  if (base::win::IsDeviceUsedAsATablet(nullptr))
    return POINTER_TYPE_COARSE;

  bool is_touch_device_present = IsTouchDevicePresent();

  if (GetSystemMetrics(SM_MOUSEPRESENT) == 0 && !is_touch_device_present)
    return POINTER_TYPE_NONE;

  int available_pointer_types = POINTER_TYPE_FINE;
  if (is_touch_device_present)
    available_pointer_types |= POINTER_TYPE_COARSE;

  return available_pointer_types;
}

// This method follows the same logic as above but with hover types.
int GetAvailableHoverTypes() {
  if (base::win::IsDeviceUsedAsATablet(nullptr))
    return HOVER_TYPE_NONE;

  if (GetSystemMetrics(SM_MOUSEPRESENT) != 0)
    return HOVER_TYPE_HOVER;

  return HOVER_TYPE_NONE;
}

TouchScreensAvailability GetTouchScreensAvailability() {
  if (!IsTouchDevicePresent())
    return TouchScreensAvailability::NONE;

  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return IsTouchDevicePresent() ? GetSystemMetrics(SM_MAXIMUMTOUCHES) : 0;
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
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
    std::transform(pointer_devices->cbegin(), pointer_devices->cend(),
                   std::back_inserter(result), &ToPointerDevice);
  }
  return result;
}

}  // namespace ui
