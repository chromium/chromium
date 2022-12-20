// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_
#define UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_

#include "base/containers/fixed_flat_map.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

// A map between top row keys to function keys.
inline constexpr auto kLayout2TopRowKeyToFKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_BROWSER_BACK, KeyboardCode::VKEY_F1},
        {KeyboardCode::VKEY_BROWSER_FORWARD, KeyboardCode::VKEY_F2},
        {KeyboardCode::VKEY_BROWSER_REFRESH, KeyboardCode::VKEY_F3},
        {KeyboardCode::VKEY_ZOOM, KeyboardCode::VKEY_F4},
        {KeyboardCode::VKEY_MEDIA_LAUNCH_APP1, KeyboardCode::VKEY_F5},
        {KeyboardCode::VKEY_BRIGHTNESS_DOWN, KeyboardCode::VKEY_F6},
        {KeyboardCode::VKEY_BRIGHTNESS_UP, KeyboardCode::VKEY_F7},
        {KeyboardCode::VKEY_VOLUME_MUTE, KeyboardCode::VKEY_F8},
        {KeyboardCode::VKEY_VOLUME_DOWN, KeyboardCode::VKEY_F9},
        {KeyboardCode::VKEY_VOLUME_UP, KeyboardCode::VKEY_F10},
    });

// A map between six pack keys to system keys.
inline constexpr auto kSixPackKeyToSystemKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_DELETE, KeyboardCode::VKEY_BACK},
        {KeyboardCode::VKEY_HOME, KeyboardCode::VKEY_LEFT},
        {KeyboardCode::VKEY_UP, KeyboardCode::VKEY_PRIOR},
        {KeyboardCode::VKEY_END, KeyboardCode::VKEY_RIGHT},
        {KeyboardCode::VKEY_NEXT, KeyboardCode::VKEY_DOWN},
        {KeyboardCode::VKEY_INSERT, KeyboardCode::VKEY_BACK},
    });

// A keyboard util API to provide various keyboard capability information, such
// as top row key layout, existence of certain keys, what is top right key, etc.
class KeyboardCapability {
 public:
  enum class DeviceType {
    kDeviceUnknown = 0,
    kDeviceInternalKeyboard,
    kDeviceExternalAppleKeyboard,
    kDeviceExternalChromeOsKeyboard,
    kDeviceExternalGenericKeyboard,
    kDeviceExternalUnknown,
    kDeviceHotrodRemote,
    kDeviceVirtualCoreKeyboard,  // X-server generated events.
  };

  enum class KeyboardTopRowLayout {
    // The original Chrome OS Layout:
    // Browser Back, Browser Forward, Refresh, Full Screen, Overview,
    // Brightness Down, Brightness Up, Mute, Volume Down, Volume Up.
    kKbdTopRowLayout1 = 1,
    kKbdTopRowLayoutDefault = kKbdTopRowLayout1,
    kKbdTopRowLayoutMin = kKbdTopRowLayout1,
    // 2017 keyboard layout: Browser Forward is gone and Play/Pause
    // key is added between Brightness Up and Mute.
    kKbdTopRowLayout2 = 2,
    // Keyboard layout and handling for Wilco.
    kKbdTopRowLayoutWilco = 3,
    kKbdTopRowLayoutDrallion = 4,

    // Handling for all keyboards that support supplying a custom layout
    // via sysfs attribute (aka Vivaldi). See crbug.com/1076241
    kKbdTopRowLayoutCustom = 5,
    kKbdTopRowLayoutMax = kKbdTopRowLayoutCustom
  };

  KeyboardCapability() = default;
  KeyboardCapability(const KeyboardCapability&) = delete;
  KeyboardCapability& operator=(const KeyboardCapability&) = delete;
  ~KeyboardCapability() = default;

  // Check if a key code is one of the six pack keys.
  static bool IsSixPackKey(const KeyboardCode& key_code);

  // Check if a key code is one of the top row keys.
  // TODO(zhangwenyu): Support all 4 legacy layouts and custom vivaldi layouts.
  bool IsTopRowKey(const ui::KeyboardCode& key_code) const;
};

}  // namespace ui

#endif  // UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_
