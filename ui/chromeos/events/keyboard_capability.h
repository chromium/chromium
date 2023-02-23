// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_
#define UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_

#include "base/containers/fixed_flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

// Keyboard layout1 map between top row keys to function keys.
inline constexpr auto kLayout1TopRowKeyToFKeyMap =
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

// Keyboard layout2 map between top row keys to function keys.
inline constexpr auto kLayout2TopRowKeyToFKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_BROWSER_BACK, KeyboardCode::VKEY_F1},
        {KeyboardCode::VKEY_BROWSER_REFRESH, KeyboardCode::VKEY_F2},
        {KeyboardCode::VKEY_ZOOM, KeyboardCode::VKEY_F3},
        {KeyboardCode::VKEY_MEDIA_LAUNCH_APP1, KeyboardCode::VKEY_F4},
        {KeyboardCode::VKEY_BRIGHTNESS_DOWN, KeyboardCode::VKEY_F5},
        {KeyboardCode::VKEY_BRIGHTNESS_UP, KeyboardCode::VKEY_F6},
        {KeyboardCode::VKEY_MEDIA_PLAY_PAUSE, KeyboardCode::VKEY_F7},
        {KeyboardCode::VKEY_VOLUME_MUTE, KeyboardCode::VKEY_F8},
        {KeyboardCode::VKEY_VOLUME_DOWN, KeyboardCode::VKEY_F9},
        {KeyboardCode::VKEY_VOLUME_UP, KeyboardCode::VKEY_F10},
    });

// Keyboard wilco/drallion map between top row keys to function keys.
// TODO(zhangwenyu): Both F3 and F12 map to VKEY_ZOOM for wilco. Handle edge
// case when creating the top row accelerator alias for VKEY_ZOOM key.
inline constexpr auto kLayoutWilcoDrallionTopRowKeyToFKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_BROWSER_BACK, KeyboardCode::VKEY_F1},
        {KeyboardCode::VKEY_BROWSER_REFRESH, KeyboardCode::VKEY_F2},
        {KeyboardCode::VKEY_ZOOM, KeyboardCode::VKEY_F3},
        {KeyboardCode::VKEY_MEDIA_LAUNCH_APP1, KeyboardCode::VKEY_F4},
        {KeyboardCode::VKEY_BRIGHTNESS_DOWN, KeyboardCode::VKEY_F5},
        {KeyboardCode::VKEY_BRIGHTNESS_UP, KeyboardCode::VKEY_F6},
        {KeyboardCode::VKEY_VOLUME_MUTE, KeyboardCode::VKEY_F7},
        {KeyboardCode::VKEY_VOLUME_DOWN, KeyboardCode::VKEY_F8},
        {KeyboardCode::VKEY_VOLUME_UP, KeyboardCode::VKEY_F9},
    });

// A map between six pack keys to system keys.
inline constexpr auto kSixPackKeyToSystemKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_DELETE, KeyboardCode::VKEY_BACK},
        {KeyboardCode::VKEY_HOME, KeyboardCode::VKEY_LEFT},
        {KeyboardCode::VKEY_PRIOR, KeyboardCode::VKEY_UP},
        {KeyboardCode::VKEY_END, KeyboardCode::VKEY_RIGHT},
        {KeyboardCode::VKEY_NEXT, KeyboardCode::VKEY_DOWN},
        {KeyboardCode::VKEY_INSERT, KeyboardCode::VKEY_BACK},
    });

// A reversed map between six pack keys to system keys. The only exception is
// the [Back], since it maps back to both [Delete] and [Insert].
inline constexpr auto kReversedSixPackKeyToSystemKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_LEFT, KeyboardCode::VKEY_HOME},
        {KeyboardCode::VKEY_UP, KeyboardCode::VKEY_PRIOR},
        {KeyboardCode::VKEY_RIGHT, KeyboardCode::VKEY_END},
        {KeyboardCode::VKEY_DOWN, KeyboardCode::VKEY_NEXT},
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

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the top_row_keys_are_fKeys prefs has changed.
    virtual void OnTopRowKeysAreFKeysChanged() = 0;
  };

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    virtual void AddObserver(Observer* observer) = 0;

    virtual void RemoveObserver(Observer* observer) = 0;

    virtual bool TopRowKeysAreFKeys() const = 0;

    virtual void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) = 0;
  };

  explicit KeyboardCapability(std::unique_ptr<Delegate> delegate);
  KeyboardCapability(const KeyboardCapability&) = delete;
  KeyboardCapability& operator=(const KeyboardCapability&) = delete;
  ~KeyboardCapability();

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  // Returns true if the target would prefer to receive raw
  // function keys instead of having them rewritten into back, forward,
  // brightness, volume, etc. or if the user has specified that they desire
  // top-row keys to be treated as function keys globally.
  bool TopRowKeysAreFKeys() const;

  // Enable or disable top row keys as F-Keys.
  void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) const;

  // Check if a key code is one of the six pack keys.
  static bool IsSixPackKey(const KeyboardCode& key_code);

  // Check if a key code is one of the reversed six pack keys.
  // A reversed six pack key is either [Back] or one of the keys in
  // kReversedSixPackKeyToSystemKeyMap.
  static bool IsReversedSixPackKey(const KeyboardCode& key_code);

  // Find the mapped function key if the given key code is a top row key for the
  // given keyboard.
  // TODO(zhangwenyu): Support custom vivaldi layouts.
  absl::optional<KeyboardCode> GetMappedFKeyIfExists(
      const KeyboardCode& key_code,
      const InputDevice& keyboard) const;

  // Check if a keyboard has a launcher button rather than a search button.
  // TODO(zhangwenyu): Handle command key and window key cases.
  bool HasLauncherButton(
      const absl::optional<InputDevice>& keyboard = absl::nullopt);

 private:
  std::unique_ptr<Delegate> delegate_;
};

}  // namespace ui

#endif  // UI_CHROMEOS_EVENTS_KEYBOARD_CAPABILITY_H_
