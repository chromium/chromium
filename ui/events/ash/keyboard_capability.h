// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_
#define UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/ash/modifier_split_dogfood_controller.h"
#include "ui/events/ash/mojom/meta_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/top_row_action_keys.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

static const TopRowActionKey kLayout1TopRowActionKeys[] = {
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
};

static const TopRowActionKey kLayout2TopRowActionKeys[] = {
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
};

static const TopRowActionKey kLayoutWilcoDrallionTopRowActionKeys[] = {
    TopRowActionKey::kBack,
    TopRowActionKey::kRefresh,
    TopRowActionKey::kFullscreen,
    TopRowActionKey::kOverview,
    TopRowActionKey::kScreenBrightnessDown,
    TopRowActionKey::kScreenBrightnessUp,
    TopRowActionKey::kVolumeMute,
    TopRowActionKey::kVolumeDown,
    TopRowActionKey::kVolumeUp,
};

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

// A map between six pack keys to search system keys.
inline constexpr auto kSixPackKeyToSearchSystemKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_DELETE, KeyboardCode::VKEY_BACK},
        {KeyboardCode::VKEY_HOME, KeyboardCode::VKEY_LEFT},
        {KeyboardCode::VKEY_PRIOR, KeyboardCode::VKEY_UP},
        {KeyboardCode::VKEY_END, KeyboardCode::VKEY_RIGHT},
        {KeyboardCode::VKEY_NEXT, KeyboardCode::VKEY_DOWN},
        {KeyboardCode::VKEY_INSERT, KeyboardCode::VKEY_BACK},
    });

// A map between six pack keys to function keys.
inline constexpr auto kSixPackKeyToFnKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_DELETE, KeyboardCode::VKEY_BACK},
        {KeyboardCode::VKEY_HOME, KeyboardCode::VKEY_LEFT},
        {KeyboardCode::VKEY_PRIOR, KeyboardCode::VKEY_UP},
        {KeyboardCode::VKEY_END, KeyboardCode::VKEY_RIGHT},
        {KeyboardCode::VKEY_NEXT, KeyboardCode::VKEY_DOWN},
    });

// A map between six pack keys to alt system keys.
inline constexpr auto kSixPackKeyToAltSystemKeyMap =
    base::MakeFixedFlatMap<KeyboardCode, KeyboardCode>({
        {KeyboardCode::VKEY_DELETE, KeyboardCode::VKEY_BACK},
        {KeyboardCode::VKEY_HOME, KeyboardCode::VKEY_UP},
        {KeyboardCode::VKEY_PRIOR, KeyboardCode::VKEY_UP},
        {KeyboardCode::VKEY_END, KeyboardCode::VKEY_DOWN},
        {KeyboardCode::VKEY_NEXT, KeyboardCode::VKEY_DOWN},
    });

// A keyboard util API to provide various keyboard capability information, such
// as top row key layout, existence of certain keys, what is top right key, etc.
class KeyboardCapability : public InputDeviceEventObserver {
 public:
  using ScanCodeToEvdevKeyConverter =
      base::RepeatingCallback<std::optional<uint32_t>(const base::ScopedFD& fd,
                                                      uint32_t scancode)>;
  enum class DeviceType {
    kDeviceUnknown = 0,
    kDeviceInternalKeyboard,
    kDeviceInternalRevenKeyboard,
    kDeviceExternalAppleKeyboard,
    kDeviceExternalChromeOsKeyboard,
    kDeviceExternalNullTopRowChromeOsKeyboard,
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

  struct KeyboardInfo {
    KeyboardInfo();
    KeyboardInfo(KeyboardInfo&&);
    KeyboardInfo& operator=(KeyboardInfo&&);
    KeyboardInfo(const KeyboardInfo&) = delete;
    KeyboardInfo& operator=(const KeyboardInfo&) = delete;
    ~KeyboardInfo();

    DeviceType device_type = DeviceType::kDeviceInternalKeyboard;
    KeyboardTopRowLayout top_row_layout =
        KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
    std::vector<uint32_t> top_row_scan_codes;
    std::vector<TopRowActionKey> top_row_action_keys;
  };

  KeyboardCapability();
  explicit KeyboardCapability(ScanCodeToEvdevKeyConverter converter);
  KeyboardCapability(const KeyboardCapability&) = delete;
  KeyboardCapability& operator=(const KeyboardCapability&) = delete;
  ~KeyboardCapability() override;

  // TODO: get rid of this. Equivalent to
  // std::make_unique<KeyboardCapability>(), and it is no longer stub.
  static std::unique_ptr<KeyboardCapability> CreateStubKeyboardCapability();

  // Generates an `EventDeviceInfo` from a given input device.
  static std::unique_ptr<EventDeviceInfo> CreateEventDeviceInfoFromInputDevice(
      const KeyboardDevice& keyboard);

  // Converts from the given `key_code` to the corresponding meaning in
  // `TopRowActionKey` enum.
  static std::optional<TopRowActionKey> ConvertToTopRowActionKey(
      ui::KeyboardCode key_code);

  // Converts the given `action_key` to the corresponding `KeyboardCode` VKEY.
  static std::optional<KeyboardCode> ConvertToKeyboardCode(
      TopRowActionKey action_key);

  // Check if a key code is one of the top row keys.
  static bool IsTopRowKey(const KeyboardCode& key_code);

  // Check if a key code is one of the six pack keys.
  static bool IsSixPackKey(const KeyboardCode& key_code);

  // Find the mapped function key if the given key code is a top row key for
  // the given keyboard.
  // TODO(zhangwenyu): Support custom vivaldi layouts.
  std::optional<KeyboardCode> GetMappedFKeyIfExists(
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

  // Check if the keycode is either the F11 or F12 function key.
  static bool IsF11OrF12(ui::KeyboardCode code);

  // Returns the set of modifier keys present on the given keyboard.
  std::vector<mojom::ModifierKey> GetModifierKeys(
      const KeyboardDevice& keyboard) const;
  std::vector<mojom::ModifierKey> GetModifierKeys(int device_id) const;

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
  bool HasAssistantKey(int device_id) const;
  bool HasAssistantKeyOnAnyKeyboard() const;

  // Check if the CapsLock key exists on the given keyboard.
  bool HasCapsLockKey(const KeyboardDevice& keyboard) const;

  // Check if the Function key exists on the given keyboard.
  bool HasFunctionKey(const KeyboardDevice& keyboard) const;
  bool HasFunctionKey(int device_id) const;
  bool HasFunctionKeyOnAnyKeyboard() const;

  // Check if the RightAlt key exists on the given keyboard.
  bool HasRightAltKey(const KeyboardDevice& keyboard) const;
  bool HasRightAltKey(int device_id) const;

  // Check if the RightAlt key exists, but only for on OOBE screen.
  bool HasRightAltKeyForOobe(const KeyboardDevice& keyboard) const;
  bool HasRightAltKeyForOobe(int device_id) const;

  // Returns the appropriate meta key present on the given keyboard.
  ui::mojom::MetaKey GetMetaKey(const KeyboardDevice& keyboard) const;
  ui::mojom::MetaKey GetMetaKey(int device_id) const;

  // Returns the meta key to display in the UI to represent the overall current
  // keyboard situation. This will only return either Launcher, Search, or
  // LauncherRefresh.
  ui::mojom::MetaKey GetMetaKeyToDisplay() const;

  // Whether or not to use the updated icons for the keyboard.
  bool UseRefreshedIcons() const;

  // Finds the keyboard with the corresponding  `device_id` and checks its
  // `DeviceType` to determine if it's a split modifier keyboard.
  bool IsSplitModifierKeyboard(const KeyboardDevice& keyboard) const;

  // Finds the keyboard with the corresponding  `device_id` and checks its
  // `DeviceType` to determine if it's a ChromeOS keyboard.
  bool IsChromeOSKeyboard(int device_id) const;

  // Gets the corresponding function key for the given `action_key` on the
  // given `keyboard`.
  std::optional<KeyboardCode> GetCorrespondingFunctionKey(
      const KeyboardDevice& keyboard,
      TopRowActionKey action_key) const;

  // Gets the corresponding action key for the given `key_code` which must be an
  // F-Key in the range of F1 to F24 for the given `keyboard`
  std::optional<TopRowActionKey> GetCorrespondingActionKeyForFKey(
      const KeyboardDevice& keyboard,
      KeyboardCode key_code) const;

  const std::vector<TopRowActionKey>* GetTopRowActionKeys(
      const KeyboardDevice& keyboard) const;
  const std::vector<TopRowActionKey>* GetTopRowActionKeys(int device_id) const;

  // Whether or not the given keyboard is a split modifier keyboard and
  // qualifies to forcibly enable features.
  bool IsSplitModifierKeyboardForOverride(const KeyboardDevice& keyboard) const;

  void SetBoardNameForTesting(const std::string& board_name);

  const base::flat_map<int, KeyboardInfo>& keyboard_info_map() const {
    return keyboard_info_map_;
  }

  bool IsModifierSplitEnabled() const {
    return modifier_split_dogfood_controller_->IsEnabled();
  }

  void ForceEnableFeature();

  void ResetModifierSplitDogfoodControllerForTesting();

 private:
  const KeyboardInfo* GetKeyboardInfo(const KeyboardDevice& keyboard) const;
  void TrimKeyboardInfoMap();

  ScanCodeToEvdevKeyConverter scan_code_to_evdev_key_converter_;

  // Stores event device info objects so they do not need to be constructed
  // multiple times. This is mutable to allow caching results from the APIs
  // which are effectively const.
  mutable base::flat_map<int, KeyboardInfo> keyboard_info_map_;

  // Whether or not to disable "trimming" which means the `keyboard_info_map_`
  // will not remove entries when they are disconnected.
  bool should_disable_trimming_ = false;

  // Board name of the current ChromeOS device.
  std::string board_name_;

  std::unique_ptr<ModifierSplitDogfoodController>
      modifier_split_dogfood_controller_;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_CAPABILITY_H_
