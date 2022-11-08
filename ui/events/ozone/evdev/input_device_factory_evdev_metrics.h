// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_METRICS_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_METRICS_H_

#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"

namespace ui {

const char kMouseAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Mouse";
const char kPointingStickAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.PointingStick";
const char kTouchpadAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Touchpad";
const char kTouchscreenAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Touchscreen";
const char kStylusAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Stylus";
const char kGamepadAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Gamepad";
const char kKeyboardAttachmentTypeHistogramName[] =
    "ChromeOS.Inputs.AttachmentType.Keyboard";

const char kInternalAttachmentFormHistogramName[] =
    "ChromeOS.Inputs.AttachmentForm.Internal";
const char kUsbAttachmentFormHistogramName[] =
    "ChromeOS.Inputs.AttachmentForm.Usb";
const char kBluetoothAttachmentFormHistogramName[] =
    "ChromeOS.Inputs.AttachmentForm.Bluetooth";
const char kUnknownAttachmentFormHistogramName[] =
    "ChromeOS.Inputs.AttachmentForm.Unknown";

enum class AttachmentType {
  kInternal = 0,
  kUsb = 1,
  kBluetooth = 2,
  kUnknown = 3,
  kMaxValue = kUnknown
};

enum class AttachmentForm {
  kMouse = 0,
  kPointingStick = 1,
  kTouchpad = 2,
  kTouchscreen = 3,
  kStylus = 4,
  kGamepad = 5,
  kKeyboard = 6,
  kMaxValue = kKeyboard
};

class COMPONENT_EXPORT(EVDEV) InputDeviceFactoryEvdevMetrics {
 public:
  InputDeviceFactoryEvdevMetrics();
  ~InputDeviceFactoryEvdevMetrics();

  void OnDeviceAttach(const EventConverterEvdev* converter);

 private:
  void RecordAttachmentType(const std::string& histogram_name,
                            AttachmentType type);
  void RecordAttachmentForm(const std::string& histogram_name,
                            AttachmentForm type);
  AttachmentType ConvertInputTypeToAttachmentType(InputDeviceType type);
  std::string ConvertInputTypeToHistogramName(InputDeviceType type);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_METRICS_H_