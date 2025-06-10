// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_MICROPHONE_MUTE_KEY_METRICS_H_
#define UI_EVENTS_OZONE_EVDEV_MICROPHONE_MUTE_KEY_METRICS_H_

#include "base/component_export.h"
#include "ui/events/devices/input_device.h"

namespace ui {

const char kMicrophoneMuteToggleDevicesHistogramName[] =
    "ChromeOS.Inputs.Peripherals.MicrophoneMuteToggleDevices";

enum class MicrophoneMuteToggleDevices {
  kSystemMicrophoneMuteJabra = 0,
  kSystemMicrophoneMuteLogitech = 1,
  kSystemMicrophoneMutePoly = 2,
  kSystemMicrophoneMuteSony = 3,
  kSystemMicrophoneMuteGoogle = 4,
  kSystemMicrophoneMuteOtherHasKeyboard = 5,
  kSystemMicrophoneMuteOtherNotKeyboard = 6,
  kPhoneMuteJabra = 7,
  kPhoneMuteLogitech = 8,
  kPhoneMutePoly = 9,
  kPhoneMuteSony = 10,
  kPhoneMuteGoogle = 11,
  kPhoneMuteOtherHasKeyboard = 12,
  kPhoneMuteOtherNotKeyboard = 13,
  kStartOrStopMicrophoneCaptureJabra = 14,
  kStartOrStopMicrophoneCaptureLogitech = 15,
  kStartOrStopMicrophoneCapturePoly = 16,
  kStartOrStopMicrophoneCaptureSony = 17,
  kStartOrStopMicrophoneCaptureGoogle = 18,
  kStartOrStopMicrophoneCaptureOtherHasKeyboard = 19,
  kStartOrStopMicrophoneCaptureOtherNotKeyboard = 20,
  kOtherMicrophoneScanCodeJabra = 21,
  kOtherMicrophoneScanCodeLogitech = 22,
  kOtherMicrophoneScanCodePoly = 23,
  kOtherMicrophoneScanCodeSony = 24,
  kOtherMicrophoneScanCodeGoogle = 25,
  kOtherMicrophoneScanCodeOtherHasKeyboard = 26,
  kOtherMicrophoneScanCodeOtherNotKeyboard = 27,
  kMaxValue = kOtherMicrophoneScanCodeOtherNotKeyboard
};

class COMPONENT_EXPORT(EVDEV) MicrophoneMuteKeyMetrics {
 public:
  MicrophoneMuteKeyMetrics(const InputDevice& input_device,
                           const bool has_keyboard);
  ~MicrophoneMuteKeyMetrics() = default;

  // Record metrics to know what kind of peripherals toggle system mute state.
  void RecordMicMuteKeyMetrics(unsigned int key,
                               bool down,
                               unsigned int last_scan_code);

 private:
  const unsigned int vendor_id_;
  const bool has_keyboard_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_MICROPHONE_MUTE_KEY_METRICS_H_
