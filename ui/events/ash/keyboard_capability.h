// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_
#define UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_

#include <memory>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

// TODO(dpad): Handle display mirror top row keys.
enum class TopRowActionKey {
  kNone = 0,
  kMinValue = kNone,
  kUnknown,
  kBack,
  kForward,
  kRefresh,
  kFullscreen,
  kOverview,
  kScreenshot,
  kScreenBrightnessDown,
  kScreenBrightnessUp,
  kMicrophoneMute,
  kVolumeMute,
  kVolumeDown,
  kVolumeUp,
  kKeyboardBacklightToggle,
  kKeyboardBacklightDown,
  kKeyboardBacklightUp,
  kNextTrack,
  kPreviousTrack,
  kPlayPause,
  kAllApplications,
  kEmojiPicker,
  kDictation,
  kPrivacyScreenToggle,
  kMaxValue = kPrivacyScreenToggle,
};

inline constexpr auto kLayout1TopRowActionKeys =
    base::MakeFixedFlatSet<TopRowActionKey>({
        TopRowActionKey::kBack,
        TopRowActionKey::kForward,
        TopRowActionKey::kRefresh,
        TopRowActionKey::kFullscreen,
        TopRowActionKey::kOverview,
        TopRowActionKey::kScreenBrightnessDown,
        TopRowActionKey::kScreenBrightnessUp,
        TopRowActionKey::kVolumeMute,
        TopRowActionKey::kVolumeDown,
        TopRowActionKey::kVolumeUp,
    });

inline constexpr auto kLayout2TopRowActionKeys =
    base::MakeFixedFlatSet<TopRowActionKey>({
        TopRowActionKey::kBack,
        TopRowActionKey::kRefresh,
        TopRowActionKey::kFullscreen,
        TopRowActionKey::kOverview,
        TopRowActionKey::kScreenBrightnessDown,
        TopRowActionKey::kScreenBrightnessUp,
        TopRowActionKey::kPlayPause,
        TopRowActionKey::kVolumeMute,
        TopRowActionKey::kVolumeDown,
        TopRowActionKey::kVolumeUp,
    });

inline constexpr auto kLayoutWilcoDrallionTopRowActionKeys =
    base::MakeFixedFlatSet<TopRowActionKey>({
        TopRowActionKey::kBack,
        TopRowActionKey::kRefresh,
        TopRowActionKey::kFullscreen,
        TopRowActionKey::kOverview,
        TopRowActionKey::kScreenBrightnessDown,
        TopRowActionKey::kScreenBrightnessUp,
        TopRowActionKey::kVolumeMute,
        TopRowActionKey::kVolumeDown,
        TopRowActionKey::kVolumeUp,
    });

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
// TODO(dpad): Handle privacy screen better on drallion devices.
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
class KeyboardCapability : public InputDeviceEventObserver {
 public:
  using ScanCodeToEvdevKeyConverter =
      base::RepeatingCallback<absl::optional<uint32_t>(const base::ScopedFD& fd,
                                                       uint32_t scancode)>;
  enum class DeviceType {
    kDeviceUnknown = 0,
    kDeviceInternalKeyboard,
    kDeviceInternalRevenKeyboard,
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

    virtual bool IsPrivacyScreenSupported() const = 0;

    virtual void SetPrivacyScreenSupportedForTesting(bool is_supported) = 0;
  };

  struct KeyboardInfo {
    KeyboardInfo();
    KeyboardInfo(KeyboardInfo&&);
    KeyboardInfo& operator=(KeyboardInfo&&);
    KeyboardInfo(const KeyboardInfo&) = delete;
    KeyboardInfo& operator=(const KeyboardInfo&) = delete;
    ~KeyboardInfo();

    DeviceType device_type;
    KeyboardTopRowLayout top_row_layout;
    std::vector<uint32_t> top_row_scan_codes;
    std::vector<TopRowActionKey> top_row_action_keys;
  };

  explicit KeyboardCapability(std::unique_ptr<Delegate> delegate);
  KeyboardCapability(ScanCodeToEvdevKeyConverter converter,
                     std::unique_ptr<Delegate> delegate);
  KeyboardCapability(const KeyboardCapability&) = delete;
  KeyboardCapability& operator=(const KeyboardCapability&) = delete;
  ~KeyboardCapability() override;

  static std::unique_ptr<KeyboardCapability> CreateStubKeyboardCapability();

  // Generates an `EventDeviceInfo` from a given input device.
  static std::unique_ptr<EventDeviceInfo> CreateEventDeviceInfoFromInputDevice(
      const KeyboardDevice& keyboard);

  // Converts from the given `key_code` to the corresponding meaning in
  // `TopRowActionKey` enum.
  static absl::optional<TopRowActionKey> ConvertToTopRowActionKey(
      ui::KeyboardCode key_code);

  // Converts the given `action_key` to the corresponding `KeyboardCode` VKEY.
  static absl::optional<KeyboardCode> ConvertToKeyboardCode(
      TopRowActionKey action_key);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  // Returns true if the target would prefer to receive raw
  // function keys instead of having them rewritten into back, forward,
  // brightness, volume, etc. or if the user has specified that they desire
  // top-row keys to be treated as function keys globally.
  // Only useful when InputDeviceSettingsSplit flag is disabled. Otherwise, it
  // returns non-useful data.
  bool TopRowKeysAreFKeys() const;

  // Enable or disable top row keys as F-Keys.
  void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) const;

  // Set whether the privacy screen is supported or not for testing.
  void SetPrivacyScreenSupportedForTesting(bool is_supported) const;

  // Check if a key code is one of the top row keys.
  static bool IsTopRowKey(const KeyboardCode& key_code);

  // Check if a key code is one of the six pack keys.
  static bool IsSixPackKey(const KeyboardCode& key_code);

  // Check if a key code is one of the reversed six pack keys.
  // A reversed six pack key is either [Back] or one of the keys in
  // kReversedSixPackKeyToSystemKeyMap.
  static bool IsReversedSixPackKey(const KeyboardCode& key_code);

  // Find the mapped function key if the given key code is a top row key for
  // the given keyboard.
  // TODO(zhangwenyu): Support custom vivaldi layouts.
  absl::optional<KeyboardCode> GetMappedFKeyIfExists(
      const KeyboardCode& key_code,
      const KeyboardDevice& keyboard) const;

  // Check if a keyboard has a launcher button rather than a search button.
  bool HasLauncherButton(const KeyboardDevice& keyboard) const;
  bool HasLauncherButtonOnAnyKeyboard() const;

  // Check if a keyboard has a six pack key.
  static bool HasSixPackKey(const KeyboardDevice& keyboard);

  // Check if any of the connected keyboards has a six pack key.
  static bool HasSixPackOnAnyKeyboard();

  // Check if the keycode is a function key.
  static bool IsFunctionKey(ui::KeyboardCode code);

  // Check if the keycode is a top-row action key.
  static bool IsTopRowActionKey(ui::KeyboardCode code);

  // Returns the set of modifier keys present on the given keyboard.
  std::vector<mojom::ModifierKey> GetModifierKeys(
      const KeyboardDevice& keyboard) const;

  // Returns the device type of the given keyboard.
  DeviceType GetDeviceType(const KeyboardDevice& keyboard) const;
  DeviceType GetDeviceType(int device_id) const;

  // Returns the device's top row layout.
  KeyboardTopRowLayout GetTopRowLayout(const KeyboardDevice& keyboard) const;
  KeyboardTopRowLayout GetTopRowLayout(int device_id) const;

  // Returns the device's top row scan codes. If the device does not have a
  // custom top row, the returned list will be null or empty.
  const std::vector<uint32_t>* GetTopRowScanCodes(
      const KeyboardDevice& keyboard) const;
  const std::vector<uint32_t>* GetTopRowScanCodes(int device_id) const;

  // Takes a `KeyboardInfo` to use for testing the passed in keyboard.
  void SetKeyboardInfoForTesting(const KeyboardDevice& keyboard, KeyboardInfo);

  // Disables "trimming" which means the `keyboard_info_map_` will not remove
  // entries when they are disconnected.
  void DisableKeyboardInfoTrimmingForTesting();

  // InputDeviceEventObserver:
  void OnDeviceListsComplete() override;
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Check if a specific key event exists on a given keyboard.
  bool HasKeyEvent(const KeyboardCode& key_code,
                   const KeyboardDevice& keyboard) const;

  // Check if any of the connected keyboards has a specific key event.
  bool HasKeyEventOnAnyKeyboard(const KeyboardCode& key_code) const;

  // Check if a given `action_key` exists on the given keyboard.
  bool HasTopRowActionKey(const KeyboardDevice& keyboard,
                          TopRowActionKey action_key) const;
  bool HasTopRowActionKeyOnAnyKeyboard(TopRowActionKey action_key) const;

  // Check if the globe key exists on the given keyboard.
  bool HasGlobeKey(const KeyboardDevice& keyboard) const;
  bool HasGlobeKeyOnAnyKeyboard() const;

  // Check if the calculator key exists on the given keyboard.
  bool HasCalculatorKey(const KeyboardDevice& keyboard) const;
  bool HasCalculatorKeyOnAnyKeyboard() const;

  // Check if the privacy screen key exists on the given keyboard.
  bool HasPrivacyScreenKey(const KeyboardDevice& keyboard) const;
  bool HasPrivacyScreenKeyOnAnyKeyboard() const;

  // Check if the browser search key exists on the given keyboard.
  bool HasBrowserSearchKey(const KeyboardDevice& keyboard) const;
  bool HasBrowserSearchKeyOnAnyKeyboard() const;

  // Check if the help key exists on the given keyboard.
  bool HasHelpKey(const KeyboardDevice& keyboard) const;
  bool HasHelpKeyOnAnyKeyboard() const;

  // Check if the settings key exists on the given keyboard.
  bool HasSettingsKey(const KeyboardDevice& keyboard) const;
  bool HasSettingsKeyOnAnyKeyboard() const;

  // Check if the given keyboard has media keys including:
  // - Media Rewind
  // - Media Fastforward
  // - Media Play
  // - Media Pause
  // These keys do not exist on any internal chromeos keyboards, but are likely
  // to potentially exist on external keyboards.
  bool HasMediaKeys(const KeyboardDevice& keyboard) const;
  bool HasMediaKeysOnAnyKeyboard() const;

  // Check if the assistant key exists on the given keyboard.
  bool HasAssistantKey(const KeyboardDevice& keyboard) const;
  bool HasAssistantKeyOnAnyKeyboard() const;

  // Check if the CapsLock key exists on the given keyboard.
  bool HasCapsLockKey(const KeyboardDevice& keyboard) const;

  // Gets the corresponding function key for the given `action_key` on the
  // given `keyboard`.
  absl::optional<KeyboardCode> GetCorrespondingFunctionKey(
      const KeyboardDevice& keyboard,
      TopRowActionKey action_key) const;

  // Gets the corresponding action key for the given `key_code` which must be an
  // F-Key in the range of F1 to F24 for the given `keyboard`
  absl::optional<TopRowActionKey> GetCorrespondingActionKeyForFKey(
      const KeyboardDevice& keyboard,
      KeyboardCode key_code) const;

  const std::vector<TopRowActionKey>* GetTopRowActionKeys(
      const KeyboardDevice& keyboard);

  const base::flat_map<int, KeyboardInfo>& keyboard_info_map() const {
    return keyboard_info_map_;
  }

 private:
  const KeyboardInfo* GetKeyboardInfo(const KeyboardDevice& keyboard) const;
  void TrimKeyboardInfoMap();

  bool IsChromeOSKeyboard(const ui::KeyboardDevice& keyboard) const;

  ScanCodeToEvdevKeyConverter scan_code_to_evdev_key_converter_;

  // Stores event device info objects so they do not need to be constructed
  // multiple times. This is mutable to allow caching results from the APIs
  // which are effectively const.
  mutable base::flat_map<int, KeyboardInfo> keyboard_info_map_;
  std::unique_ptr<Delegate> delegate_;

  // Whether or not to disable "trimming" which means the `keyboard_info_map_`
  // will not remove entries when they are disconnected.
  bool should_disable_trimming_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_
