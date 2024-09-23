// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_TEST_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_TEST_UTIL_H_

#include <linux/input.h>
#include <stddef.h>

#include "base/memory/raw_ptr_exclusion.h"

namespace ui {

class EventDeviceInfo;

struct DeviceAbsoluteAxis {
  unsigned int code;
  input_absinfo absinfo;
};

struct DeviceCapabilities {
  // Full sysfs path (readlink -f /sys/class/input/event*)
  const char* path;

  // EVIOCGNAME (/sys/class/input/*/device/name)
  const char* name;

  // EVIOCGPHYS (/sys/class/input/*/device/phys)
  const char* phys;

  // EVIOCGUNIQ (/sys/class/input/*/device/uniq)
  const char* uniq;

  // EVIOCGID (/sys/class/input/*/device/id)
  const char* bustype;
  const char* vendor;
  const char* product;
  const char* version;

  // EVIOCGPROP (/sys/class/input/*/device/properties)
  // 64-bit groups.
  const char* prop;

  // EVIOCGBIT (/sys/class/input/*/device/capabilities)
  // 64-bit groups.
  const char* ev;
  const char* key;
  const char* rel;
  const char* abs;
  const char* msc;
  const char* sw;
  const char* led;
  const char* ff;

  // EVIOCGABS.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const DeviceAbsoluteAxis* abs_axis;
  size_t abs_axis_count;

  // (/sys/class/input/*/device/device/function_row_physmap)
  // Device kernel attribute.
  const char* kbd_function_row_physmap;

  // (udevadm info -q property /sys/class/input/* | grep
  // CROS_KEYBOARD_TOP_ROW_LAYOUT) Udev property, note that this comes from udev
  // rules and only exists in user-space.
  const char* kbd_top_row_layout;
};

bool CapabilitiesToDeviceInfo(const DeviceCapabilities& capabilities,
                              EventDeviceInfo* devinfo);

extern const DeviceCapabilities kXboxGamepad;
extern const DeviceCapabilities kHJCGamepad;
extern const DeviceCapabilities kiBuffaloGamepad;
extern const DeviceCapabilities kBaskingTouchScreen;
extern const DeviceCapabilities kEveTouchScreen;
extern const DeviceCapabilities kLinkKeyboard;
extern const DeviceCapabilities kLinkTouchscreen;
extern const DeviceCapabilities kLinkWithToolTypeTouchscreen;
extern const DeviceCapabilities kLinkTouchpad;
extern const DeviceCapabilities kRedrixTouchpad;
extern const DeviceCapabilities kMorphiusPointingStick;
extern const DeviceCapabilities kMorphiusTabletModeSwitch;
extern const DeviceCapabilities kHpUsbKeyboard;
extern const DeviceCapabilities kHpUsbKeyboard_Extra;
extern const DeviceCapabilities kLogitechUsbMouse;
extern const DeviceCapabilities kMimoTouch2Touchscreen;
extern const DeviceCapabilities kWacomIntuosPtS_Pen;
extern const DeviceCapabilities kWacomIntuosPtS_Finger;
extern const DeviceCapabilities kLogitechTouchKeyboardK400;
extern const DeviceCapabilities kElo_TouchSystems_2700;
extern const DeviceCapabilities kWilsonBeachActiveStylus;
extern const DeviceCapabilities kEveStylus;
extern const DeviceCapabilities kHammerKeyboard;
extern const DeviceCapabilities kHammerTouchpad;
extern const DeviceCapabilities kIlitekTP_Mouse;
extern const DeviceCapabilities kIlitekTP;
extern const DeviceCapabilities kSideVolumeButton;
extern const DeviceCapabilities kNocturneTouchScreen;
extern const DeviceCapabilities kNocturneStylus;
extern const DeviceCapabilities kKohakuTouchscreen;
extern const DeviceCapabilities kKohakuStylus;
extern const DeviceCapabilities kXboxElite;
extern const DeviceCapabilities kDrallionStylus;
extern const DeviceCapabilities kDellActivePenButton;
extern const DeviceCapabilities kDrallionKeyboard;
extern const DeviceCapabilities kPuffMicrophoneMuteSwitch;
extern const DeviceCapabilities kDrawciaStylusGarage;
extern const DeviceCapabilities kEveKeyboard;
extern const DeviceCapabilities kSarienKeyboard;
extern const DeviceCapabilities kWoomaxKeyboard;
extern const DeviceCapabilities kDrobitKeyboard;
extern const DeviceCapabilities kDrobitNumberpad;
extern const DeviceCapabilities kLogitechKeyboardK120;
extern const DeviceCapabilities kMicrosoftBluetoothNumberPad;
extern const DeviceCapabilities kDellLatitudeE6510Touchpad;
extern const DeviceCapabilities kHPProBook6560bTouchpad;
extern const DeviceCapabilities kJinlonKeyboard;
extern const DeviceCapabilities kSymbolTechBarcodeScanner;
extern const DeviceCapabilities kRexHeatmapTouchScreen;
extern const DeviceCapabilities kSplitModifierKeyboard;
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_TEST_UTIL_H_
