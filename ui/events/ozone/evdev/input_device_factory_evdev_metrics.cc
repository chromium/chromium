// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ui {

void InputDeviceFactoryEvdevMetrics::RecordAttachmentType(
    const std::string& histogram_name,
    AttachmentType type) {
  base::UmaHistogramEnumeration(histogram_name, type);
}

void InputDeviceFactoryEvdevMetrics::RecordAttachmentForm(
    const std::string& histogram_name,
    AttachmentForm type) {
  base::UmaHistogramEnumeration(histogram_name, type);
}

AttachmentType InputDeviceFactoryEvdevMetrics::ConvertInputTypeToAttachmentType(
    InputDeviceType type) {
  switch (type) {
    case InputDeviceType::INPUT_DEVICE_INTERNAL:
      return AttachmentType::kInternal;
    case InputDeviceType::INPUT_DEVICE_USB:
      return AttachmentType::kUsb;
    case InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return AttachmentType::kBluetooth;
    case InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return AttachmentType::kUnknown;
    default:
      return AttachmentType::kUnknown;
  }
}

std::string InputDeviceFactoryEvdevMetrics::ConvertInputTypeToHistogramName(
    InputDeviceType type) {
  switch (type) {
    case InputDeviceType::INPUT_DEVICE_INTERNAL:
      return kInternalAttachmentFormHistogramName;
    case InputDeviceType::INPUT_DEVICE_USB:
      return kUsbAttachmentFormHistogramName;
    case InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return kBluetoothAttachmentFormHistogramName;
    case InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return kUnknownAttachmentFormHistogramName;
    default:
      return kUnknownAttachmentFormHistogramName;
  }
}

void InputDeviceFactoryEvdevMetrics::OnDeviceAttach(
    const EventConverterEvdev* converter) {
  std::tuple<bool, std::string, AttachmentForm> vals[] = {
      std::make_tuple(converter->HasMouse(), kMouseAttachmentTypeHistogramName,
                      AttachmentForm::kMouse),
      std::make_tuple(converter->HasPointingStick(),
                      kPointingStickAttachmentTypeHistogramName,
                      AttachmentForm::kPointingStick),
      std::make_tuple(converter->HasTouchpad(),
                      kTouchpadAttachmentTypeHistogramName,
                      AttachmentForm::kTouchpad),
      std::make_tuple(converter->HasTouchscreen(),
                      kTouchscreenAttachmentTypeHistogramName,
                      AttachmentForm::kTouchscreen),
      std::make_tuple(converter->HasPen(), kStylusAttachmentTypeHistogramName,
                      AttachmentForm::kStylus),
      std::make_tuple(converter->HasGamepad(),
                      kGamepadAttachmentTypeHistogramName,
                      AttachmentForm::kGamepad),
      std::make_tuple(converter->HasKeyboard(),
                      kKeyboardAttachmentTypeHistogramName,
                      AttachmentForm::kKeyboard),
  };

  // A converter can be multiple forms, therefore the same converter can
  // generate multiple metric entries.
  for (auto& val : vals) {
    const auto& [has_form, histogram_name, attachment_form] = val;
    if (has_form) {
      RecordAttachmentType(histogram_name,
                           ConvertInputTypeToAttachmentType(converter->type()));
      RecordAttachmentForm(ConvertInputTypeToHistogramName(converter->type()),
                           attachment_form);
    }
  }
}

InputDeviceFactoryEvdevMetrics::InputDeviceFactoryEvdevMetrics() = default;

InputDeviceFactoryEvdevMetrics::~InputDeviceFactoryEvdevMetrics() = default;

}  // namespace ui