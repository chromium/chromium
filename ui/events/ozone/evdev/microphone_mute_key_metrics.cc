// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/microphone_mute_key_metrics.h"

#include <linux/input.h>

#include "base/metrics/histogram_functions.h"

namespace ui {

namespace {
// Telephony Device Page (0x0B) Phone Mute (0x2F) is defined in
// https://usb.org/sites/default/files/hut1_5.pdf.
const int kTelephonyDevicePhoneMute = 0x0b002f;
// Generic Desktop Page (0x01) System Microphone Mute (0xA9) is defined in
// https://usb.org/sites/default/files/hut1_5.pdf.
const int kGenericDesktopSystemMicrophoneMute = 0x0100a9;
// Consumer Page (0x0C) Start or Stop Microphone capture (0xD5) is defined in
// https://usb.org/sites/default/files/hut1_5.pdf.
const int kConsumerStartOrStopMicrophoneCapture = 0x0c00d5;
// Popular Vendor IDs.
const int kJabraVendorID = 0x0b0e;
const int kLogitechVendorID = 0x046d;
const int kPolyVendorID = 0x047f;
const int kSonyVendorID = 0x054c;
const int kGoogleVendorID = 0x18d1;
}  // namespace

MicrophoneMuteKeyMetrics::MicrophoneMuteKeyMetrics(
    const InputDevice& input_device,
    const bool has_keyboard)
    : vendor_id_(input_device.vendor_id), has_keyboard_(has_keyboard) {}

void MicrophoneMuteKeyMetrics::RecordMicMuteKeyMetrics(
    unsigned int key,
    bool down,
    unsigned int last_scan_code) {
  // Add metrics to know what kind of peripherals toggle system mute state.
  if (key == KEY_MICMUTE && down) {
    switch (last_scan_code) {
      case kTelephonyDevicePhoneMute:
        switch (vendor_id_) {
          case kJabraVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kPhoneMuteJabra);
            break;
          case kLogitechVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kPhoneMuteLogitech);
            break;
          case kPolyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kPhoneMutePoly);
            break;
          case kSonyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kPhoneMuteSony);
            break;
          case kGoogleVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kPhoneMuteGoogle);
            break;
          default:
            if (has_keyboard_) {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::kPhoneMuteOtherHasKeyboard);
            } else {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::kPhoneMuteOtherNotKeyboard);
            }
        }
        break;
      case kGenericDesktopSystemMicrophoneMute:
        switch (vendor_id_) {
          case kJabraVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kSystemMicrophoneMuteJabra);
            break;
          case kLogitechVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kSystemMicrophoneMuteLogitech);
            break;
          case kPolyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kSystemMicrophoneMutePoly);
            break;
          case kSonyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kSystemMicrophoneMuteSony);
            break;
          case kGoogleVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kSystemMicrophoneMuteGoogle);
            break;
          default:
            if (has_keyboard_) {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kSystemMicrophoneMuteOtherHasKeyboard);
            } else {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kSystemMicrophoneMuteOtherNotKeyboard);
            }
        }
        break;
      case kConsumerStartOrStopMicrophoneCapture:
        switch (vendor_id_) {
          case kJabraVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::
                    kStartOrStopMicrophoneCaptureJabra);
            break;
          case kLogitechVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::
                    kStartOrStopMicrophoneCaptureLogitech);
            break;
          case kPolyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kStartOrStopMicrophoneCapturePoly);
            break;
          case kSonyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kStartOrStopMicrophoneCaptureSony);
            break;
          case kGoogleVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::
                    kStartOrStopMicrophoneCaptureGoogle);
            break;
          default:
            if (has_keyboard_) {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kStartOrStopMicrophoneCaptureOtherHasKeyboard);
            } else {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kStartOrStopMicrophoneCaptureOtherNotKeyboard);
            }
        }
        break;
      default:
        switch (vendor_id_) {
          case kJabraVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kOtherMicrophoneScanCodeJabra);
            break;
          case kLogitechVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kOtherMicrophoneScanCodeLogitech);
            break;
          case kPolyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kOtherMicrophoneScanCodePoly);
            break;
          case kSonyVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kOtherMicrophoneScanCodeSony);
            break;
          case kGoogleVendorID:
            base::UmaHistogramEnumeration(
                kMicrophoneMuteToggleDevicesHistogramName,
                MicrophoneMuteToggleDevices::kOtherMicrophoneScanCodeGoogle);
            break;
          default:
            if (has_keyboard_) {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kOtherMicrophoneScanCodeOtherHasKeyboard);
            } else {
              base::UmaHistogramEnumeration(
                  kMicrophoneMuteToggleDevicesHistogramName,
                  MicrophoneMuteToggleDevices::
                      kOtherMicrophoneScanCodeOtherNotKeyboard);
            }
        }
    }
  }
}

}  // namespace ui
