// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_capability.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <cstring>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

namespace {

using KeyboardTopRowLayout = KeyboardCapability::KeyboardTopRowLayout;
using DeviceType = KeyboardCapability::DeviceType;

// Represents scancode value seen in scan code mapping which denotes that the
// FKey is missing on the physical device.
const int kCustomAbsentScanCode = 0x00;

// Hotrod controller vendor/product ids.
const int kHotrodRemoteVendorId = 0x0471;
const int kHotrodRemoteProductId = 0x21cc;

constexpr char kLayoutProperty[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kCustomTopRowLayoutAttribute[] = "function_row_physmap";
constexpr char kCustomTopRowLayoutProperty[] = "FUNCTION_ROW_PHYSMAP";

constexpr KeyboardCode kFunctionKeys[] = {
    VKEY_F1,  VKEY_F2,  VKEY_F3,  VKEY_F4,  VKEY_F5,
    VKEY_F6,  VKEY_F7,  VKEY_F8,  VKEY_F9,  VKEY_F10,
    VKEY_F11, VKEY_F12, VKEY_F13, VKEY_F14, VKEY_F15,
};
constexpr KeyboardCode kMaxCustomTopRowLayoutFKeyCode = VKEY_F15;
constexpr size_t kNumCustomTopRowFKeys =
    (kMaxCustomTopRowLayoutFKeyCode - VKEY_F1) + 1;

class StubKeyboardCapabilityDelegate : public KeyboardCapability::Delegate {
 public:
  StubKeyboardCapabilityDelegate() = default;
  StubKeyboardCapabilityDelegate(const StubKeyboardCapabilityDelegate&) =
      delete;
  StubKeyboardCapabilityDelegate& operator=(
      const StubKeyboardCapabilityDelegate&) = delete;
  ~StubKeyboardCapabilityDelegate() override = default;

  void AddObserver(KeyboardCapability::Observer* observer) override {}
  void RemoveObserver(KeyboardCapability::Observer* observer) override {}
  bool TopRowKeysAreFKeys() const override { return false; }
  void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) override {}
  bool IsPrivacyScreenSupported() const override { return false; }
  void SetPrivacyScreenSupportedForTesting(bool is_supported) override {}
};

absl::optional<InputDevice> FindKeyboardWithId(int device_id) {
  const auto& keyboards =
      DeviceDataManager::GetInstance()->GetKeyboardDevices();
  for (const auto& keyboard : keyboards) {
    if (keyboard.id == device_id) {
      return keyboard;
    }
  }

  return absl::nullopt;
}

bool GetDeviceProperty(const base::FilePath& device_path,
                       const char* key,
                       std::string& value) {
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev.get()) {
    return false;
  }

  device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get()) {
    return false;
  }

  value = device::UdevDeviceGetPropertyValue(device.get(), key);
  return true;
}

// Parses the custom top row layout string. The string contains a space
// separated list of scan codes in hex. eg "aa ab ac" for F1, F2, F3, etc.
std::vector<uint32_t> ParseCustomTopRowLayoutScancodes(
    const std::string& layout) {
  std::vector<uint32_t> scancode_vector;

  const std::vector<std::string> scan_code_strings = base::SplitString(
      layout, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (scan_code_strings.size() == 0 ||
      scan_code_strings.size() > kNumCustomTopRowFKeys) {
    return {};
  }

  for (const auto& scan_code_string : scan_code_strings) {
    uint32_t scan_code = 0;
    if (!base::HexStringToUInt(scan_code_string, &scan_code)) {
      return {};
    }

    scancode_vector.push_back(scan_code);
  }

  return scancode_vector;
}

// Returns true if |value| is replaced with the specific device attribute value
// without getting an error. |device_path| should be obtained from the
// |InputDevice.sys_path| field.
bool GetDeviceAttributeRecursive(const base::FilePath& device_path,
                                 const char* key,
                                 std::string& value) {
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev.get()) {
    return false;
  }

  device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get()) {
    return false;
  }

  value = device::UdevDeviceRecursiveGetSysattrValue(device.get(), key);
  return true;
}

base::ScopedFD GetEventDeviceNameFd(const InputDevice& keyboard) {
  const char kDevNameProperty[] = "DEVNAME";
  std::string dev_name;
  if (!GetDeviceProperty(keyboard.sys_path, kDevNameProperty, dev_name) ||
      dev_name.empty()) {
    return base::ScopedFD();
  }

  base::ScopedFD fd(open(dev_name.c_str(), O_RDONLY));
  if (fd.get() < 0) {
    LOG(ERROR) << "Cannot open " << dev_name.c_str() << " : " << errno;
    return base::ScopedFD();
  }

  return fd;
}

absl::optional<uint32_t> ConvertScanCodeToEvdevKey(const base::ScopedFD& fd,
                                                   uint32_t scancode) {
  if (fd.get() < 0) {
    return absl::nullopt;
  }

  struct input_keymap_entry keymap_entry {
    .flags = 0, .len = sizeof(scancode), .keycode = 0
  };
  memcpy(keymap_entry.scancode, &scancode, sizeof(scancode));

  int ret = ioctl(fd.get(), EVIOCGKEYCODE_V2, &keymap_entry);
  if (ret < 0) {
    LOG(ERROR) << "Failed EVIOCGKEYCODE_V2 syscall";
    return absl::nullopt;
  }

  return keymap_entry.keycode;
}

bool GetCustomTopRowLayoutAttribute(const InputDevice& keyboard,
                                    std::string& out_prop) {
  bool result = GetDeviceAttributeRecursive(
      keyboard.sys_path, kCustomTopRowLayoutAttribute, out_prop);

  if (result && out_prop.size() > 0) {
    VLOG(1) << "Identified custom top row keyboard layout: sys_path="
            << keyboard.sys_path << " layout=" << out_prop;
    return true;
  }

  return false;
}

bool GetCustomTopRowLayout(const InputDevice& keyboard, std::string& out_prop) {
  if (GetCustomTopRowLayoutAttribute(keyboard, out_prop)) {
    return true;
  }
  return GetDeviceProperty(keyboard.sys_path, kCustomTopRowLayoutProperty,
                           out_prop);
}

std::vector<uint32_t> GetTopRowScanCodeVector(const InputDevice& keyboard) {
  std::string layout;
  if (!GetCustomTopRowLayout(keyboard, layout) || layout.empty()) {
    return {};
  }

  return ParseCustomTopRowLayoutScancodes(layout);
}

bool GetTopRowLayoutProperty(const InputDevice& keyboard_device,
                             std::string& out_prop) {
  return GetDeviceProperty(keyboard_device.sys_path, kLayoutProperty, out_prop);
}

// Parses keyboard to row layout string. Returns true if data is valid.
bool ParseKeyboardTopRowLayout(const std::string& layout_string,
                               KeyboardTopRowLayout& out_layout) {
  if (layout_string.empty()) {
    out_layout = KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
    return true;
  }

  int layout_id;
  if (!base::StringToInt(layout_string, &layout_id)) {
    LOG(WARNING) << "Failed to parse layout " << kLayoutProperty << " value '"
                 << layout_string << "'";
    return false;
  }

  if (layout_id < static_cast<int>(KeyboardTopRowLayout::kKbdTopRowLayoutMin) ||
      layout_id > static_cast<int>(KeyboardTopRowLayout::kKbdTopRowLayoutMax)) {
    LOG(WARNING) << "Invalid " << kLayoutProperty << " '" << layout_string
                 << "'";
    return false;
  }

  out_layout = static_cast<KeyboardTopRowLayout>(layout_id);
  return true;
}

// Determines the type of |keyboard_device| we are dealing with.
// |has_chromeos_top_row| argument indicates that the keyboard's top
// row has "action" keys (such as back, refresh, etc.) instead of the
// standard F1-F12 keys.
KeyboardCapability::DeviceType IdentifyKeyboardType(
    const InputDevice& keyboard_device,
    bool has_chromeos_top_row) {
  if (keyboard_device.vendor_id == kHotrodRemoteVendorId &&
      keyboard_device.product_id == kHotrodRemoteProductId) {
    VLOG(1) << "Hotrod remote '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceHotrodRemote;
  }

  if (base::EqualsCaseInsensitiveASCII(keyboard_device.name,
                                       "virtual core keyboard")) {
    VLOG(1) << "Xorg virtual '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard;
  }

  if (keyboard_device.type == INPUT_DEVICE_INTERNAL) {
    VLOG(1) << "Internal keyboard '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  }

  if (has_chromeos_top_row) {
    // If the device was tagged as having Chrome OS top row layout it must be a
    // Chrome OS keyboard.
    VLOG(1) << "External Chrome OS keyboard '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard;
  }

  const std::vector<std::string> tokens =
      base::SplitString(keyboard_device.name, " .", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  // Parse |device_name| to help classify it.
  bool found_apple = false;
  bool found_keyboard = false;
  for (const auto& token : tokens) {
    if (!found_apple && base::EqualsCaseInsensitiveASCII(token, "apple")) {
      found_apple = true;
    }
    if (!found_keyboard &&
        base::EqualsCaseInsensitiveASCII(token, "keyboard")) {
      found_keyboard = true;
    }
  }
  if (found_apple) {
    // If the |device_name| contains the two words, "apple" and "keyboard",
    // treat it as an Apple keyboard.
    if (found_keyboard) {
      VLOG(1) << "Apple keyboard '" << keyboard_device.name
              << "' connected: id=" << keyboard_device.id;
      return KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard;
    } else {
      VLOG(1) << "Apple device '" << keyboard_device.name
              << "' connected: id=" << keyboard_device.id;
      return KeyboardCapability::DeviceType::kDeviceExternalUnknown;
    }
  } else if (found_keyboard) {
    VLOG(1) << "External keyboard '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
  } else {
    VLOG(1) << "External device '" << keyboard_device.name
            << "' connected: id=" << keyboard_device.id;
    return KeyboardCapability::DeviceType::kDeviceExternalUnknown;
  }
}

std::tuple<DeviceType, KeyboardTopRowLayout, std::vector<uint32_t>>
IdentifyKeyboardInfo(const InputDevice& keyboard) {
  std::string layout_string;
  KeyboardTopRowLayout layout;
  std::vector<uint32_t> top_row_scan_codes = GetTopRowScanCodeVector(keyboard);
  if (!top_row_scan_codes.empty()) {
    layout = KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  } else if (!GetTopRowLayoutProperty(keyboard, layout_string) ||
             !ParseKeyboardTopRowLayout(layout_string, layout)) {
    return {KeyboardCapability::DeviceType::kDeviceUnknown,
            KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
            {}};
  }

  return {IdentifyKeyboardType(
              keyboard, !top_row_scan_codes.empty() || !layout_string.empty()),
          layout, std::move(top_row_scan_codes)};
}

std::vector<TopRowActionKey> IdentifyCustomTopRowActionKeys(
    const KeyboardCapability::ScanCodeToEvdevKeyConverter&
        scan_code_to_evdev_key_converter,
    const InputDevice& keyboard,
    const std::vector<uint32_t>& top_row_scan_codes) {
  base::ScopedFD fd = GetEventDeviceNameFd(keyboard);

  // TODO(dpad): Handle privacy screen in scan code mapping.
  std::vector<TopRowActionKey> top_row_action_keys;
  top_row_action_keys.reserve(top_row_scan_codes.size());
  for (const auto& scancode : top_row_scan_codes) {
    if (scancode == kCustomAbsentScanCode) {
      top_row_action_keys.push_back(TopRowActionKey::kNone);
      continue;
    }

    auto evdev_key_code = scan_code_to_evdev_key_converter.Run(fd, scancode);
    if (!evdev_key_code) {
      top_row_action_keys.push_back(TopRowActionKey::kUnknown);
      continue;
    }

    const DomCode dom_code =
        KeycodeConverter::EvdevCodeToDomCode(*evdev_key_code);
    KeyboardCode action_vkey = DomCodeToUsLayoutKeyboardCode(dom_code);
    if (action_vkey == VKEY_UNKNOWN) {
      if (dom_code == DomCode::SHOW_ALL_WINDOWS) {
        // Show all windows is through VKEY_MEDIA_LAUNCH_APP1.
        action_vkey = VKEY_MEDIA_LAUNCH_APP1;
      }
    }

    auto action_key = KeyboardCapability::ConvertToTopRowActionKey(action_vkey);
    if (action_key) {
      top_row_action_keys.push_back(*action_key);
    } else {
      top_row_action_keys.push_back(TopRowActionKey::kUnknown);
    }
  }
  return top_row_action_keys;
}

std::vector<TopRowActionKey> IdentifyTopRowActionKeys(
    const KeyboardCapability::ScanCodeToEvdevKeyConverter&
        scan_code_to_evdev_key_converter,
    const InputDevice& keyboard,
    DeviceType device_type,
    KeyboardTopRowLayout layout,
    const std::vector<uint32_t>& top_row_scan_codes) {
  switch (layout) {
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
      return {kLayout1TopRowActionKeys.begin(), kLayout1TopRowActionKeys.end()};
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
      return {kLayout2TopRowActionKeys.begin(), kLayout2TopRowActionKeys.end()};
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      return {kLayoutWilcoDrallionTopRowActionKeys.begin(),
              kLayoutWilcoDrallionTopRowActionKeys.end()};
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      return IdentifyCustomTopRowActionKeys(scan_code_to_evdev_key_converter,
                                            keyboard, top_row_scan_codes);
  }
}

bool IsInternalKeyboard(const ui::InputDevice& keyboard) {
  return keyboard.type == INPUT_DEVICE_INTERNAL;
}

}  // namespace

KeyboardCapability::KeyboardCapability(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  scan_code_to_evdev_key_converter_ =
      base::BindRepeating(&ConvertScanCodeToEvdevKey);
  DeviceDataManager::GetInstance()->AddObserver(this);
}

KeyboardCapability::KeyboardCapability(
    ScanCodeToEvdevKeyConverter scan_code_to_evdev_key_converter,
    std::unique_ptr<Delegate> delegate)
    : scan_code_to_evdev_key_converter_(
          std::move(scan_code_to_evdev_key_converter)),
      delegate_(std::move(delegate)) {
  DeviceDataManager::GetInstance()->AddObserver(this);
}

KeyboardCapability::~KeyboardCapability() {
  DeviceDataManager::GetInstance()->RemoveObserver(this);
}

KeyboardCapability::KeyboardInfo::KeyboardInfo() = default;
KeyboardCapability::KeyboardInfo::KeyboardInfo(KeyboardInfo&&) = default;
KeyboardCapability::KeyboardInfo& KeyboardCapability::KeyboardInfo::operator=(
    KeyboardInfo&&) = default;
KeyboardCapability::KeyboardInfo::~KeyboardInfo() = default;

// static
std::unique_ptr<KeyboardCapability>
KeyboardCapability::CreateStubKeyboardCapability() {
  return std::make_unique<KeyboardCapability>(
      std::make_unique<StubKeyboardCapabilityDelegate>());
}

// static
std::unique_ptr<EventDeviceInfo>
KeyboardCapability::CreateEventDeviceInfoFromInputDevice(
    const InputDevice& keyboard) {
  base::ScopedFD fd = GetEventDeviceNameFd(keyboard);
  if (fd.get() < 0) {
    return nullptr;
  }

  std::unique_ptr<EventDeviceInfo> event_device_info =
      std::make_unique<EventDeviceInfo>();
  if (!event_device_info->Initialize(fd.get(), keyboard.sys_path)) {
    LOG(ERROR) << "Failed to get device information for "
               << keyboard.sys_path.value();
    return nullptr;
  }

  return event_device_info;
}

// static
absl::optional<TopRowActionKey> KeyboardCapability::ConvertToTopRowActionKey(
    ui::KeyboardCode key_code) {
  static constexpr auto kVKeyToTopRowActionKeyMap =
      base::MakeFixedFlatMap<ui::KeyboardCode, TopRowActionKey>({
          {VKEY_BROWSER_BACK, TopRowActionKey::kBack},
          {VKEY_BROWSER_FORWARD, TopRowActionKey::kForward},
          {VKEY_BROWSER_REFRESH, TopRowActionKey::kRefresh},
          {VKEY_ZOOM, TopRowActionKey::kFullscreen},
          {VKEY_MEDIA_LAUNCH_APP1, TopRowActionKey::kOverview},
          {VKEY_SNAPSHOT, TopRowActionKey::kScreenshot},
          {VKEY_BRIGHTNESS_DOWN, TopRowActionKey::kScreenBrightnessDown},
          {VKEY_BRIGHTNESS_UP, TopRowActionKey::kScreenBrightnessUp},
          {VKEY_MICROPHONE_MUTE_TOGGLE, TopRowActionKey::kMicrophoneMute},
          {VKEY_VOLUME_MUTE, TopRowActionKey::kVolumeMute},
          {VKEY_VOLUME_DOWN, TopRowActionKey::kVolumeDown},
          {VKEY_VOLUME_UP, TopRowActionKey::kVolumeUp},
          {VKEY_KBD_BACKLIGHT_TOGGLE,
           TopRowActionKey::kKeyboardBacklightToggle},
          {VKEY_KBD_BRIGHTNESS_DOWN, TopRowActionKey::kKeyboardBacklightDown},
          {VKEY_KBD_BRIGHTNESS_UP, TopRowActionKey::kKeyboardBacklightUp},
          {VKEY_MEDIA_NEXT_TRACK, TopRowActionKey::kNextTrack},
          {VKEY_MEDIA_PREV_TRACK, TopRowActionKey::kPreviousTrack},
          {VKEY_MEDIA_PLAY_PAUSE, TopRowActionKey::kPlayPause},
          {VKEY_ALL_APPLICATIONS, TopRowActionKey::kAllApplications},
          {VKEY_EMOJI_PICKER, TopRowActionKey::kEmojiPicker},
          {VKEY_DICTATE, TopRowActionKey::kDictation},
      });
  const auto* action_key = kVKeyToTopRowActionKeyMap.find(key_code);
  return (action_key != kVKeyToTopRowActionKeyMap.end())
             ? absl::make_optional<TopRowActionKey>(action_key->second)
             : absl::nullopt;
}

void KeyboardCapability::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void KeyboardCapability::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

bool KeyboardCapability::TopRowKeysAreFKeys() const {
  return delegate_->TopRowKeysAreFKeys();
}

void KeyboardCapability::SetTopRowKeysAsFKeysEnabledForTesting(
    bool enabled) const {
  CHECK_IS_TEST();
  delegate_->SetTopRowKeysAsFKeysEnabledForTesting(enabled);  // IN-TEST
}

void KeyboardCapability::SetPrivacyScreenSupportedForTesting(
    bool is_supported) const {
  CHECK_IS_TEST();
  delegate_->SetPrivacyScreenSupportedForTesting(is_supported);  // IN-TEST
}

// static
bool KeyboardCapability::IsSixPackKey(const KeyboardCode& key_code) {
  return base::Contains(kSixPackKeyToSystemKeyMap, key_code);
}

// static
bool KeyboardCapability::IsReversedSixPackKey(const KeyboardCode& key_code) {
  // [Back] maps back to both [Delete] and [Insert].
  return base::Contains(kReversedSixPackKeyToSystemKeyMap, key_code) ||
         key_code == ui::KeyboardCode::VKEY_BACK;
}

absl::optional<KeyboardCode> KeyboardCapability::GetMappedFKeyIfExists(
    const KeyboardCode& key_code,
    const InputDevice& keyboard) const {
  // TODO(zhangwenyu): Cache the layout for currently connected keyboards and
  // observe the keyboard changes.
  KeyboardTopRowLayout layout = GetTopRowLayout(keyboard);
  switch (layout) {
    case KeyboardTopRowLayout::kKbdTopRowLayout1:
      if (kLayout1TopRowKeyToFKeyMap.contains(key_code)) {
        return kLayout1TopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayout2:
      if (kLayout2TopRowKeyToFKeyMap.contains(key_code)) {
        return kLayout2TopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      if (kLayoutWilcoDrallionTopRowKeyToFKeyMap.contains(key_code)) {
        return kLayoutWilcoDrallionTopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      // TODO(zhangwenyu): Handle custom vivaldi layout.
      return absl::nullopt;
  }

  return absl::nullopt;
}

absl::optional<KeyboardCode> KeyboardCapability::GetCorrespondingFunctionKey(
    const InputDevice& keyboard,
    TopRowActionKey action_key) const {
  auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return absl::nullopt;
  }

  auto iter =
      base::ranges::find(keyboard_info->top_row_action_keys, action_key);
  if (iter == keyboard_info->top_row_action_keys.end()) {
    return absl::nullopt;
  }

  return kFunctionKeys[std::distance(keyboard_info->top_row_action_keys.begin(),
                                     iter)];
}

bool KeyboardCapability::HasLauncherButton(
    const absl::optional<InputDevice>& keyboard) {
  // Use current implementation. If keyboard is provided, launcher button
  // depends on if this keyboard is layout2 type. If keyboard is not provided,
  // launcher button depends on if any keyboard in DeviceDataManager is layout2
  // type.
  // TODO(zhangwenyu): Handle edge cases.
  if (!keyboard.has_value()) {
    for (const InputDevice& keyboard_iter :
         DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
      if (GetTopRowLayout(keyboard_iter) ==
          KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2) {
        return true;
      }
    }
    return false;
  }

  return GetTopRowLayout(keyboard.value()) ==
         KeyboardTopRowLayout::kKbdTopRowLayout2;
}

// static
bool KeyboardCapability::IsTopRowKey(const KeyboardCode& key_code) {
  // A set that includes all top row keys from different keyboards.
  static const base::NoDestructor<base::flat_set<KeyboardCode>>
      top_row_action_keys({
          KeyboardCode::VKEY_BROWSER_BACK,
          KeyboardCode::VKEY_BROWSER_FORWARD,
          KeyboardCode::VKEY_BROWSER_REFRESH,
          KeyboardCode::VKEY_ZOOM,
          KeyboardCode::VKEY_MEDIA_LAUNCH_APP1,
          KeyboardCode::VKEY_ALL_APPLICATIONS,
          KeyboardCode::VKEY_SNAPSHOT,
          KeyboardCode::VKEY_BRIGHTNESS_DOWN,
          KeyboardCode::VKEY_BRIGHTNESS_UP,
          KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE,
          KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE,
          KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
          KeyboardCode::VKEY_VOLUME_MUTE,
          KeyboardCode::VKEY_VOLUME_DOWN,
          KeyboardCode::VKEY_VOLUME_UP,
          KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE,
          KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN,
          KeyboardCode::VKEY_KBD_BRIGHTNESS_UP,
          KeyboardCode::VKEY_MEDIA_NEXT_TRACK,
          KeyboardCode::VKEY_MEDIA_PREV_TRACK,
      });
  return base::Contains(*top_row_action_keys, key_code);
}

// static
bool KeyboardCapability::HasSixPackKey(const InputDevice& keyboard) {
  // If the keyboard is an internal keyboard, return false. Otherwise, return
  // true. This is correct for most of the keyboards. Edge cases will be handled
  // later.
  // TODO(zhangwenyu): handle edge cases when this logic doesn't apply.
  return keyboard.type != InputDeviceType::INPUT_DEVICE_INTERNAL;
}

// static
bool KeyboardCapability::HasSixPackOnAnyKeyboard() {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (ui::KeyboardCapability::HasSixPackKey(keyboard)) {
      return true;
    }
  }
  return false;
}

// static
bool KeyboardCapability::IsFunctionKey(ui::KeyboardCode code) {
  return ui::KeyboardCode::VKEY_F1 <= code &&
         code <= ui::KeyboardCode::VKEY_F24;
}

bool KeyboardCapability::IsTopRowActionKey(ui::KeyboardCode code) {
  // TODO(jimmyxgong): This is based off of the Layout1, Layout2, Wilco/Drallion
  // mappings with some additional keys. This is not a complete list.
  static constexpr auto kTopRowKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
      ui::KeyboardCode::VKEY_BROWSER_BACK,
      ui::KeyboardCode::VKEY_BROWSER_FORWARD,
      ui::KeyboardCode::VKEY_BROWSER_REFRESH,
      ui::KeyboardCode::VKEY_ZOOM,
      ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP1,
      ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN,
      ui::KeyboardCode::VKEY_BRIGHTNESS_UP,
      ui::KeyboardCode::VKEY_VOLUME_MUTE,
      ui::KeyboardCode::VKEY_VOLUME_UP,
      ui::KeyboardCode::VKEY_VOLUME_DOWN,
      ui::KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE,
      ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE,
      ui::KeyboardCode::VKEY_SNAPSHOT,
      ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
      ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN,
      ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_UP,
  });
  return base::Contains(kTopRowKeys, code);
}

std::vector<mojom::ModifierKey> KeyboardCapability::GetModifierKeys(
    const InputDevice& keyboard) const {
  // This set of modifier keys is available on every keyboard.
  std::vector<mojom::ModifierKey> modifier_keys = {
      mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
      mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
      mojom::ModifierKey::kAlt,
  };

  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return modifier_keys;
  }

  // CapsLock exists on all non-chromeos keyboards.
  if (keyboard_info->device_type !=
          KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard &&
      keyboard_info->device_type !=
          KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    modifier_keys.push_back(mojom::ModifierKey::kCapsLock);
  }

  // Assistant key can be checked by querying evdev properties.
  if (keyboard_info && keyboard_info->event_device_info &&
      keyboard_info->event_device_info->HasKeyEvent(KEY_ASSISTANT)) {
    modifier_keys.push_back(mojom::ModifierKey::kAssistant);
  }

  return modifier_keys;
}

DeviceType KeyboardCapability::GetDeviceType(
    const InputDevice& keyboard) const {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return DeviceType::kDeviceUnknown;
  }

  return keyboard_info->device_type;
}

DeviceType KeyboardCapability::GetDeviceType(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return DeviceType::kDeviceUnknown;
  }

  return GetDeviceType(*keyboard);
}

KeyboardTopRowLayout KeyboardCapability::GetTopRowLayout(
    const InputDevice& keyboard) const {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
  }

  return keyboard_info->top_row_layout;
}

KeyboardTopRowLayout KeyboardCapability::GetTopRowLayout(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
  }

  return GetTopRowLayout(*keyboard);
}

void KeyboardCapability::SetKeyboardInfoForTesting(const InputDevice& keyboard,
                                                   KeyboardInfo keyboard_info) {
  keyboard_info_map_.insert_or_assign(keyboard.id, std::move(keyboard_info));
}

const KeyboardCapability::KeyboardInfo* KeyboardCapability::GetKeyboardInfo(
    const InputDevice& keyboard) const {
  auto iter = keyboard_info_map_.find(keyboard.id);
  if (iter != keyboard_info_map_.end()) {
    return &iter->second;
  }

  // Insert new keyboard info into the map.
  auto& keyboard_info = keyboard_info_map_[keyboard.id];
  std::tie(keyboard_info.device_type, keyboard_info.top_row_layout,
           keyboard_info.top_row_scan_codes) = IdentifyKeyboardInfo(keyboard);
  keyboard_info.top_row_action_keys = IdentifyTopRowActionKeys(
      scan_code_to_evdev_key_converter_, keyboard, keyboard_info.device_type,
      keyboard_info.top_row_layout, keyboard_info.top_row_scan_codes);
  // Enable only when flag is enabled to avoid crashing while problem is
  // addressed. This issue exists the `EventDeviceInfo` objects are only allowed
  // to be created on a thread that allows blocking. See b/272960076
  if (ash::features::IsInputDeviceSettingsSplitEnabled() ||
      features::IsShortcutCustomizationAppEnabled()) {
    keyboard_info.event_device_info =
        CreateEventDeviceInfoFromInputDevice(keyboard);
  }

  // If we are unable to identify the device, erase the entry from the map.
  if (keyboard_info.device_type == DeviceType::kDeviceUnknown) {
    keyboard_info_map_.erase(keyboard.id);
    return nullptr;
  }

  return &keyboard_info;
}

const std::vector<uint32_t>* KeyboardCapability::GetTopRowScanCodes(
    const InputDevice& keyboard) const {
  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return nullptr;
  }

  return &keyboard_info->top_row_scan_codes;
}

const std::vector<uint32_t>* KeyboardCapability::GetTopRowScanCodes(
    int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard.has_value()) {
    return nullptr;
  }

  return GetTopRowScanCodes(*keyboard);
}

bool KeyboardCapability::HasGlobeKey(const InputDevice& keyboard) const {
  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return false;
  }

  // TODO(dpad): This is not quite right, some external keyboards have it as
  // well.
  // Globe key only exists on drallion or wilco devices.
  return keyboard_info->top_row_layout ==
             KeyboardTopRowLayout::kKbdTopRowLayoutDrallion ||
         keyboard_info->top_row_layout ==
             KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
}

bool KeyboardCapability::HasGlobeKeyOnAnyKeyboard() const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasGlobeKey(keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasCalculatorKey(const InputDevice& keyboard) const {
  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return false;
  }

  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasCalculatorKeyOnAnyKeyboard() const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasCalculatorKey(keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasPrivacyScreenKey(
    const InputDevice& keyboard) const {
  return GetTopRowLayout(keyboard) ==
             KeyboardTopRowLayout::kKbdTopRowLayoutDrallion &&
         delegate_->IsPrivacyScreenSupported();
}

bool KeyboardCapability::HasPrivacyScreenKeyOnAnyKeyboard() const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasPrivacyScreenKey(keyboard)) {
      return true;
    }
  }
  return false;
}

void KeyboardCapability::OnDeviceListsComplete() {
  TrimKeyboardInfoMap();
}

void KeyboardCapability::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & InputDeviceEventObserver::kKeyboard) {
    TrimKeyboardInfoMap();
  }
}

void KeyboardCapability::TrimKeyboardInfoMap() {
  auto sorted_keyboards =
      DeviceDataManager::GetInstance()->GetKeyboardDevices();
  base::ranges::sort(sorted_keyboards, [](const ui::InputDevice& device1,
                                          const ui::InputDevice& device2) {
    return device1.id < device2.id;
  });

  // Generate a vector with only the device ids from the
  // `keyboard_info_map_` map. Guaranteed to be sorted as flat_map is always
  // in sorted order by key.
  std::vector<int> cached_event_device_info_ids;
  cached_event_device_info_ids.reserve(keyboard_info_map_.size());
  base::ranges::transform(keyboard_info_map_,
                          std::back_inserter(cached_event_device_info_ids),
                          [](const auto& pair) { return pair.first; });
  DCHECK(base::ranges::is_sorted(cached_event_device_info_ids));

  // Compares the `cached_event_device_info_ids` to the id field of
  // `sorted_keyboards`. Ids that are in `cached_event_device_info_ids` but not
  // in `sorted_keyboards` are inserted into `keyboard_ids_to_remove`.
  // `sorted_keyboards` and `cached_event_device_info_ids` must be sorted.
  std::vector<int> keyboard_ids_to_remove;
  base::ranges::set_difference(
      cached_event_device_info_ids, sorted_keyboards,
      std::back_inserter(keyboard_ids_to_remove),
      /*Comp=*/base::ranges::less(),
      /*Proj1=*/base::identity(),
      /*Proj2=*/[](const InputDevice& device) { return device.id; });

  for (const auto& id : keyboard_ids_to_remove) {
    keyboard_info_map_.erase(id);
  }
}

bool KeyboardCapability::HasKeyEvent(const KeyboardCode& key_code,
                                     const InputDevice& keyboard) const {
  // Handle top row keys.
  if (IsTopRowKey(key_code)) {
    KeyboardTopRowLayout layout = GetTopRowLayout(keyboard);
    switch (layout) {
      case KeyboardTopRowLayout::kKbdTopRowLayout1:
        return kLayout1TopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayout2:
        return kLayout2TopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
        return kLayoutWilcoDrallionTopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
        // TODO(zhangwenyu): Handle custom vivaldi layout.
        return true;
    }
  }

  // Handle six pack keys.
  if (IsSixPackKey(key_code)) {
    return HasSixPackKey(keyboard);
  }

  // Handle assistant key.
  if (key_code == KeyboardCode::VKEY_ASSISTANT) {
    const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
    return keyboard_info && keyboard_info->event_device_info &&
           keyboard_info->event_device_info->HasKeyEvent(KEY_ASSISTANT);
  }

  // TODO(zhangwenyu): check other specific keys, e.g. assistant key.
  return true;
}

bool KeyboardCapability::HasKeyEventOnAnyKeyboard(
    const KeyboardCode& key_code) const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasKeyEvent(key_code, keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasTopRowActionKey(const InputDevice& keyboard,
                                            TopRowActionKey action_key) const {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return kLayout1TopRowActionKeys.contains(action_key);
  }

  return base::Contains(keyboard_info->top_row_action_keys, action_key);
}

bool KeyboardCapability::HasTopRowActionKeyOnAnyKeyboard(
    TopRowActionKey action_key) const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasTopRowActionKey(keyboard, action_key)) {
      return true;
    }
  }
  return false;
}

}  // namespace ui
