// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ash/keyboard_capability.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>

#include <cstring>
#include <functional>
#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
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
#include "base/system/sys_info.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_info_metrics.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/ash/modifier_split_dogfood_controller.h"
#include "ui/events/ash/mojom/meta_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
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

struct VendorProductId {
  uint16_t vendor_id;
  uint16_t product_id;
  constexpr bool operator<(const VendorProductId& other) const {
    return vendor_id == other.vendor_id ? product_id < other.product_id
                                        : vendor_id < other.vendor_id;
  }
};

// Represents scancode value seen in scan code mapping which denotes that the
// FKey is missing on the physical device.
const int kCustomAbsentScanCode = 0x00;

// Represents the "null" scancode used to represent the opting out of Meta +
// F-Key rewrites functionality.
const int kCustomNullScanCode = 0xC0000;

// Hotrod controller vendor/product ids.
const int kHotrodRemoteVendorId = 0x0471;
const int kHotrodRemoteProductId = 0x21cc;

constexpr auto kRightAltBlocklist =
    base::MakeFixedFlatSet<std::string_view>({"eve", "nocturne", "atlas"});

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

// Map used to convert VKEY -> TopRowActionKey and vice versa.
constexpr auto kVKeyToTopRowActionKeyMap =
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
        {VKEY_KBD_BACKLIGHT_TOGGLE, TopRowActionKey::kKeyboardBacklightToggle},
        {VKEY_KBD_BRIGHTNESS_DOWN, TopRowActionKey::kKeyboardBacklightDown},
        {VKEY_KBD_BRIGHTNESS_UP, TopRowActionKey::kKeyboardBacklightUp},
        {VKEY_MEDIA_NEXT_TRACK, TopRowActionKey::kNextTrack},
        {VKEY_MEDIA_PREV_TRACK, TopRowActionKey::kPreviousTrack},
        {VKEY_MEDIA_PLAY_PAUSE, TopRowActionKey::kPlayPause},
        {VKEY_ALL_APPLICATIONS, TopRowActionKey::kAllApplications},
        {VKEY_EMOJI_PICKER, TopRowActionKey::kEmojiPicker},
        {VKEY_DICTATE, TopRowActionKey::kDictation},
        {VKEY_PRIVACY_SCREEN_TOGGLE, TopRowActionKey::kPrivacyScreenToggle},
        {VKEY_ACCESSIBILITY, TopRowActionKey::kAccessibility},
    });

// Some ChromeOS compatible keyboards have a capslock key.
constexpr auto kChromeOSKeyboardsWithCapsLock =
    base::MakeFixedFlatSet<VendorProductId>({
        {0x046d, 0xb370}  // Logitech Signature K650
    });

std::optional<KeyboardDevice> FindKeyboardWithId(int device_id) {
  const auto& keyboards =
      DeviceDataManager::GetInstance()->GetKeyboardDevices();
  for (const auto& keyboard : keyboards) {
    if (keyboard.id == device_id) {
      return keyboard;
    }
  }

  return std::nullopt;
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
// |KeyboardDevice.sys_path| field.
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

base::ScopedFD GetEventDeviceNameFd(const KeyboardDevice& keyboard) {
  const char kDevNameProperty[] = "DEVNAME";
  std::string dev_name;
  if (!GetDeviceProperty(keyboard.sys_path, kDevNameProperty, dev_name) ||
      dev_name.empty()) {
    return base::ScopedFD();
  }

  base::ScopedFD fd(open(dev_name.c_str(), O_RDONLY));
  if (fd.get() < 0) {
    PLOG(ERROR) << "Cannot open " << dev_name.c_str();
    return base::ScopedFD();
  }

  return fd;
}

std::optional<uint32_t> ConvertScanCodeToEvdevKey(const base::ScopedFD& fd,
                                                  uint32_t scancode) {
  if (fd.get() < 0) {
    return std::nullopt;
  }

  struct input_keymap_entry keymap_entry {
    .flags = 0, .len = sizeof(scancode), .keycode = 0
  };
  memcpy(keymap_entry.scancode, &scancode, sizeof(scancode));

  int ret = ioctl(fd.get(), EVIOCGKEYCODE_V2, &keymap_entry);
  if (ret < 0) {
    LOG(ERROR) << "Failed EVIOCGKEYCODE_V2 syscall";
    return std::nullopt;
  }

  return keymap_entry.keycode;
}

bool GetCustomTopRowLayoutAttribute(const KeyboardDevice& keyboard,
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

bool GetCustomTopRowLayout(const KeyboardDevice& keyboard,
                           std::string& out_prop) {
  if (GetCustomTopRowLayoutAttribute(keyboard, out_prop)) {
    return true;
  }
  return GetDeviceProperty(keyboard.sys_path, kCustomTopRowLayoutProperty,
                           out_prop);
}

std::vector<uint32_t> GetTopRowScanCodeVector(const KeyboardDevice& keyboard) {
  std::string layout;
  if (!GetCustomTopRowLayout(keyboard, layout) || layout.empty()) {
    return {};
  }

  return ParseCustomTopRowLayoutScancodes(layout);
}

bool GetTopRowLayoutProperty(const KeyboardDevice& keyboard_device,
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
    const KeyboardDevice& keyboard_device,
    bool has_chromeos_top_row,
    bool has_null_top_row) {
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
    return ash::switches::IsRevenBranding()
               ? KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard
               : KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  }

  if (has_chromeos_top_row) {
    if (has_null_top_row) {
      VLOG(1) << "External Null Top Row keyboard '" << keyboard_device.name
              << "' connected: id=" << keyboard_device.id;
      return KeyboardCapability::DeviceType::
          kDeviceExternalNullTopRowChromeOsKeyboard;
    }

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
IdentifyKeyboardInfo(const KeyboardDevice& keyboard) {
  std::string layout_string;
  KeyboardTopRowLayout layout;
  std::vector<uint32_t> top_row_scan_codes = GetTopRowScanCodeVector(keyboard);
  bool null_top_row = false;
  if (!top_row_scan_codes.empty()) {
    layout = KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
    null_top_row =
        base::ranges::all_of(top_row_scan_codes, [](const uint32_t scancode) {
          return scancode == kCustomNullScanCode;
        });
  } else if (!GetTopRowLayoutProperty(keyboard, layout_string) ||
             !ParseKeyboardTopRowLayout(layout_string, layout)) {
    return {KeyboardCapability::DeviceType::kDeviceUnknown,
            KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
            {}};
  }

  return {IdentifyKeyboardType(
              keyboard, !top_row_scan_codes.empty() || !layout_string.empty(),
              null_top_row),
          layout, std::move(top_row_scan_codes)};
}

std::vector<TopRowActionKey> IdentifyCustomTopRowActionKeys(
    const KeyboardCapability::ScanCodeToEvdevKeyConverter&
        scan_code_to_evdev_key_converter,
    const KeyboardDevice& keyboard,
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
    const KeyboardDevice& keyboard,
    DeviceType device_type,
    KeyboardTopRowLayout layout,
    const std::vector<uint32_t>& top_row_scan_codes) {
  switch (layout) {
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
      return std::vector<TopRowActionKey>(std::begin(kLayout1TopRowActionKeys),
                                          std::end(kLayout1TopRowActionKeys));
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
      return std::vector<TopRowActionKey>(std::begin(kLayout2TopRowActionKeys),
                                          std::end(kLayout2TopRowActionKeys));
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      return std::vector<TopRowActionKey>(
          std::begin(kLayoutWilcoDrallionTopRowActionKeys),
          std::end(kLayoutWilcoDrallionTopRowActionKeys));
    case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      return IdentifyCustomTopRowActionKeys(scan_code_to_evdev_key_converter,
                                            keyboard, top_row_scan_codes);
  }
}

bool IsInternalKeyboard(const ui::KeyboardDevice& keyboard) {
  return keyboard.type == INPUT_DEVICE_INTERNAL;
}

bool HasExternalKeyboardConnected() {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (!keyboard.suspected_keyboard_imposter &&
        !IsInternalKeyboard(keyboard)) {
      return true;
    }
  }
  return false;
}

}  // namespace

KeyboardCapability::KeyboardCapability()
    : scan_code_to_evdev_key_converter_(
          base::BindRepeating(&ConvertScanCodeToEvdevKey)),
      board_name_(base::ToLowerASCII(base::SysInfo::HardwareModelName())),
      modifier_split_dogfood_controller_(
          std::make_unique<ModifierSplitDogfoodController>()) {
  DeviceDataManager::GetInstance()->AddObserver(this);
}

KeyboardCapability::KeyboardCapability(
    ScanCodeToEvdevKeyConverter scan_code_to_evdev_key_converter)
    : scan_code_to_evdev_key_converter_(
          std::move(scan_code_to_evdev_key_converter)),
      modifier_split_dogfood_controller_(
          std::make_unique<ModifierSplitDogfoodController>()) {
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
  return std::make_unique<KeyboardCapability>();
}

// static
std::unique_ptr<EventDeviceInfo>
KeyboardCapability::CreateEventDeviceInfoFromInputDevice(
    const KeyboardDevice& keyboard) {
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
std::optional<TopRowActionKey> KeyboardCapability::ConvertToTopRowActionKey(
    ui::KeyboardCode key_code) {
  const auto action_key = kVKeyToTopRowActionKeyMap.find(key_code);
  return (action_key != kVKeyToTopRowActionKeyMap.end())
             ? std::make_optional<TopRowActionKey>(action_key->second)
             : std::nullopt;
}

// static
std::optional<KeyboardCode> KeyboardCapability::ConvertToKeyboardCode(
    TopRowActionKey action_key) {
  for (const auto& [key_code, mapped_action_key] : kVKeyToTopRowActionKeyMap) {
    if (mapped_action_key == action_key) {
      return key_code;
    }
  }
  return std::nullopt;
}

// static
bool KeyboardCapability::IsSixPackKey(const KeyboardCode& key_code) {
  return base::Contains(kSixPackKeyToSearchSystemKeyMap, key_code);
}

std::optional<KeyboardCode> KeyboardCapability::GetMappedFKeyIfExists(
    const KeyboardCode& key_code,
    const KeyboardDevice& keyboard) const {
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
      return std::nullopt;
  }

  return std::nullopt;
}

std::optional<KeyboardCode> KeyboardCapability::GetCorrespondingFunctionKey(
    const KeyboardDevice& keyboard,
    TopRowActionKey action_key) const {
  auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return std::nullopt;
  }

  auto iter =
      base::ranges::find(keyboard_info->top_row_action_keys, action_key);
  if (iter == keyboard_info->top_row_action_keys.end()) {
    return std::nullopt;
  }

  return kFunctionKeys[std::distance(keyboard_info->top_row_action_keys.begin(),
                                     iter)];
}

std::optional<TopRowActionKey>
KeyboardCapability::GetCorrespondingActionKeyForFKey(
    const KeyboardDevice& keyboard,
    KeyboardCode key_code) const {
  auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return std::nullopt;
  }

  if (key_code > VKEY_F24 || key_code < VKEY_F1) {
    return std::nullopt;
  }

  const size_t index = key_code - VKEY_F1;
  if (keyboard_info->top_row_action_keys.size() <= index) {
    return std::nullopt;
  }

  return keyboard_info->top_row_action_keys[index];
}

// TODO(dpad): Remove once modifier split launches.
bool KeyboardCapability::HasLauncherButton(
    const KeyboardDevice& keyboard) const {
  // TODO(dpad): This is not entirely correct. Some devices which have custom
  // top rows have a search icon on their keyboard (ie jinlon).
  // In general, only chromebooks with layout1 top rows use the search icon.
  auto top_row_layout = GetTopRowLayout(keyboard);
  switch (top_row_layout) {
    case KeyboardTopRowLayout::kKbdTopRowLayout1:
      // Some external keyboards report the wrong layout type.
      return !IsInternalKeyboard(keyboard);
    case KeyboardTopRowLayout::kKbdTopRowLayout2:
    case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
    case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      return true;
  }
}

// TODO(dpad): Remove once modifier split launches.
bool KeyboardCapability::HasLauncherButtonOnAnyKeyboard() const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasLauncherButton(keyboard)) {
      return true;
    }
  }
  return false;
}

// static
bool KeyboardCapability::IsTopRowKey(const KeyboardCode& key_code) {
  // A set that includes all top row keys from different keyboards.
  const auto action = kVKeyToTopRowActionKeyMap.find(key_code);
  return action != kVKeyToTopRowActionKeyMap.end();
}

// static
bool KeyboardCapability::HasSixPackKey(const KeyboardDevice& keyboard) {
  // If the keyboard is an internal keyboard, return false. Otherwise, return
  // true. This is correct for most of the keyboards. Edge cases will be handled
  // later.
  // TODO(zhangwenyu): handle edge cases when this logic doesn't apply.
  return keyboard.type != InputDeviceType::INPUT_DEVICE_INTERNAL;
}

// static
bool KeyboardCapability::HasSixPackOnAnyKeyboard() {
  for (const ui::KeyboardDevice& keyboard :
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

// static
bool KeyboardCapability::IsF11OrF12(ui::KeyboardCode code) {
  return code == ui::KeyboardCode::VKEY_F11 ||
         code == ui::KeyboardCode::VKEY_F12;
}

std::vector<mojom::ModifierKey> KeyboardCapability::GetModifierKeys(
    const KeyboardDevice& keyboard) const {
  // This set of modifier keys is available on every keyboard.
  std::vector<mojom::ModifierKey> modifier_keys = {
      mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
      mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
      mojom::ModifierKey::kAlt,
  };

  if (HasCapsLockKey(keyboard)) {
    modifier_keys.push_back(mojom::ModifierKey::kCapsLock);
  }

  if (HasAssistantKey(keyboard)) {
    modifier_keys.push_back(mojom::ModifierKey::kAssistant);
  }

  if (HasFunctionKey(keyboard)) {
    modifier_keys.push_back(mojom::ModifierKey::kFunction);
  }

  if (HasRightAltKey(keyboard)) {
    modifier_keys.push_back(mojom::ModifierKey::kRightAlt);
  }

  return modifier_keys;
}

std::vector<mojom::ModifierKey> KeyboardCapability::GetModifierKeys(
    int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return {};
  }

  return GetModifierKeys(*keyboard);
}

DeviceType KeyboardCapability::GetDeviceType(
    const KeyboardDevice& keyboard) const {
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
    const KeyboardDevice& keyboard) const {
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

void KeyboardCapability::SetKeyboardInfoForTesting(
    const KeyboardDevice& keyboard,
    KeyboardInfo keyboard_info) {
  keyboard_info_map_.insert_or_assign(keyboard.id, std::move(keyboard_info));
}

void KeyboardCapability::DisableKeyboardInfoTrimmingForTesting() {
  should_disable_trimming_ = true;
}

const KeyboardCapability::KeyboardInfo* KeyboardCapability::GetKeyboardInfo(
    const KeyboardDevice& keyboard) const {
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

  // If we are unable to identify the device, erase the entry from the map.
  if (keyboard_info.device_type == DeviceType::kDeviceUnknown) {
    keyboard_info_map_.erase(keyboard.id);
    return nullptr;
  }

  // This metrics recording will happen once per keyboard per connection, since
  // GetKeyboardInfo is cached and isn't recomputed unless the keyboard
  // disconnects and reconnects.
  RecordKeyboardInfoMetrics(keyboard_info,
                            /*has_assistant_key=*/HasAssistantKey(keyboard),
                            /*has_right_alt_key=*/HasRightAltKey(keyboard));

  return &keyboard_info;
}

const std::vector<uint32_t>* KeyboardCapability::GetTopRowScanCodes(
    const KeyboardDevice& keyboard) const {
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

bool KeyboardCapability::HasGlobeKey(const KeyboardDevice& keyboard) const {
  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return false;
  }

  // TODO(jimmyxgong): VKEY_MODECHANGE (globe key) for now we should assume
  // can be available for external keyboards or Wilco/Drallion device. Will
  // need a better way to determine if the key is available in non
  // Wilco/Drallion keyboards.
  return !IsInternalKeyboard(keyboard) ||
         keyboard_info->top_row_layout ==
             KeyboardTopRowLayout::kKbdTopRowLayoutDrallion ||
         keyboard_info->top_row_layout ==
             KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
}

bool KeyboardCapability::HasGlobeKeyOnAnyKeyboard() const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasGlobeKey(keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasCalculatorKey(
    const KeyboardDevice& keyboard) const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasCalculatorKeyOnAnyKeyboard() const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return HasExternalKeyboardConnected();
}

bool KeyboardCapability::HasBrowserSearchKey(
    const KeyboardDevice& keyboard) const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasBrowserSearchKeyOnAnyKeyboard() const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return HasExternalKeyboardConnected();
}

bool KeyboardCapability::HasHelpKey(const KeyboardDevice& keyboard) const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasHelpKeyOnAnyKeyboard() const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return HasExternalKeyboardConnected();
}

bool KeyboardCapability::HasSettingsKey(const KeyboardDevice& keyboard) const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasSettingsKeyOnAnyKeyboard() const {
  // TODO(dpad): Many external keyboards do not have this key, but currently we
  // do not have a good way to detect these situations.
  return HasExternalKeyboardConnected();
}

bool KeyboardCapability::HasMediaKeys(const KeyboardDevice& keyboard) const {
  // TODO(dpad): Many external keyboards do not have these keys, but currently
  // we do not have a good way to detect these situations.
  return !IsInternalKeyboard(keyboard);
}

bool KeyboardCapability::HasMediaKeysOnAnyKeyboard() const {
  // TODO(dpad): Many external keyboards do not have these keys, but currently
  // we do not have a good way to detect these situations.
  return HasExternalKeyboardConnected();
}

const std::vector<TopRowActionKey>* KeyboardCapability::GetTopRowActionKeys(
    const KeyboardDevice& keyboard) const {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return nullptr;
  }

  return &keyboard_info->top_row_action_keys;
}

const std::vector<TopRowActionKey>* KeyboardCapability::GetTopRowActionKeys(
    int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return nullptr;
  }

  return GetTopRowActionKeys(*keyboard);
}

bool KeyboardCapability::HasAssistantKey(const KeyboardDevice& keyboard) const {
  if (HasRightAltKey(keyboard)) {
    return false;
  }

  if (ash::features::IsSplitKeyboardRefactorEnabled()) {
    return false;
  }

  // Some external keyboards falsely claim to have assistant keys. However, this
  // can be trusted for internal + ChromeOS external keyboards.
  return keyboard.has_assistant_key && IsChromeOSKeyboard(keyboard.id);
}

bool KeyboardCapability::HasAssistantKey(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return false;
  }

  return HasAssistantKey(*keyboard);
}

bool KeyboardCapability::HasAssistantKeyOnAnyKeyboard() const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasAssistantKey(keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasCapsLockKey(const KeyboardDevice& keyboard) const {
  return !IsChromeOSKeyboard(keyboard.id) ||
         kChromeOSKeyboardsWithCapsLock.contains(
             {keyboard.vendor_id, keyboard.product_id});
}

bool KeyboardCapability::HasFunctionKey(const KeyboardDevice& keyboard) const {
  if (!modifier_split_dogfood_controller_->IsEnabled()) {
    return false;
  }

  if (ash::features::IsSplitKeyboardRefactorEnabled()) {
    return true;
  }

  return ash::features::IsModifierSplitEnabled() &&
         keyboard.type == InputDeviceType::INPUT_DEVICE_INTERNAL &&
         keyboard.has_function_key;
}

bool KeyboardCapability::HasFunctionKey(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return false;
  }

  return HasFunctionKey(*keyboard);
}

bool KeyboardCapability::HasFunctionKeyOnAnyKeyboard() const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasFunctionKey(keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasRightAltKey(const KeyboardDevice& keyboard) const {
  if (!modifier_split_dogfood_controller_->IsEnabled()) {
    return false;
  }

  if (ash::features::IsSplitKeyboardRefactorEnabled()) {
    return true;
  }

  if (kRightAltBlocklist.contains(board_name_)) {
    return false;
  }

  return keyboard.type == InputDeviceType::INPUT_DEVICE_INTERNAL &&
         keyboard.has_assistant_key;
}

bool KeyboardCapability::HasRightAltKey(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return false;
  }

  return HasRightAltKey(*keyboard);
}

bool KeyboardCapability::HasRightAltKeyForOobe(
    const KeyboardDevice& keyboard) const {
  if (modifier_split_dogfood_controller_->IsEnabled()) {
    return false;
  }

  if (ash::features::IsSplitKeyboardRefactorEnabled()) {
    return true;
  }

  if (kRightAltBlocklist.contains(board_name_)) {
    return false;
  }

  return keyboard.type == InputDeviceType::INPUT_DEVICE_INTERNAL &&
         keyboard.has_assistant_key;
}

bool KeyboardCapability::HasRightAltKeyForOobe(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return false;
  }

  return HasRightAltKeyForOobe(*keyboard);
}

bool KeyboardCapability::IsSplitModifierKeyboardForOverride(
    const KeyboardDevice& keyboard) const {
  if (kRightAltBlocklist.contains(board_name_)) {
    return false;
  }

  return ash::features::IsModifierSplitEnabled() &&
         keyboard.type == InputDeviceType::INPUT_DEVICE_INTERNAL &&
         keyboard.has_function_key && keyboard.has_assistant_key;
}

ui::mojom::MetaKey KeyboardCapability::GetMetaKey(
    const KeyboardDevice& keyboard) const {
  const auto device_type = GetDeviceType(keyboard);
  switch (device_type) {
    case ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      return mojom::MetaKey::kCommand;
    case ui::KeyboardCapability::DeviceType::kDeviceUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown:
    case ui::KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard:
    case ui::KeyboardCapability::DeviceType::
        kDeviceExternalNullTopRowChromeOsKeyboard:
      return mojom::MetaKey::kExternalMeta;
    case ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
    case ui::KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case ui::KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
      break;
  };

  if (IsSplitModifierKeyboard(keyboard)) {
    return mojom::MetaKey::kLauncherRefresh;
  }

  // TODO(dpad): This is not entirely correct. Some devices which have custom
  // top rows have a search icon on their keyboard (ie jinlon).
  // In general, only chromebooks with layout1 top rows use the search icon.
  auto top_row_layout = GetTopRowLayout(keyboard);
  switch (top_row_layout) {
    case KeyboardTopRowLayout::kKbdTopRowLayout1:
      return IsInternalKeyboard(keyboard) ? mojom::MetaKey::kSearch
                                          : mojom::MetaKey::kLauncher;
    case KeyboardTopRowLayout::kKbdTopRowLayout2:
    case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
    case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      return mojom::MetaKey::kLauncher;
  }
}

ui::mojom::MetaKey KeyboardCapability::GetMetaKey(int device_id) const {
  auto keyboard = FindKeyboardWithId(device_id);
  if (!keyboard) {
    return mojom::MetaKey::kLauncher;
  }

  return GetMetaKey(*keyboard);
}

ui::mojom::MetaKey KeyboardCapability::GetMetaKeyToDisplay() const {
  ui::mojom::MetaKey current_best = ui::mojom::MetaKey::kExternalMeta;
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    const ui::mojom::MetaKey meta_key = GetMetaKey(keyboard);
    // Ordered in priority order. If a keyboard is connected with a refreshed
    // launcher key, it should have ultimate priority.
    switch (meta_key) {
      case mojom::MetaKey::kLauncherRefresh:
        current_best = mojom::MetaKey::kLauncherRefresh;
        break;
      case mojom::MetaKey::kLauncher:
        if (current_best != mojom::MetaKey::kLauncherRefresh) {
          current_best = mojom::MetaKey::kLauncher;
        }
        break;
      case mojom::MetaKey::kSearch:
        if (current_best == mojom::MetaKey::kExternalMeta) {
          current_best = mojom::MetaKey::kSearch;
        }
        break;
      case mojom::MetaKey::kExternalMeta:
      case mojom::MetaKey::kCommand:
        break;
    }
  }

  if (current_best != mojom::MetaKey::kExternalMeta &&
      current_best != mojom::MetaKey::kCommand) {
    return current_best;
  }

  // Override meta key icon for external keyboards to be the highest priority
  // icon.
  if (modifier_split_dogfood_controller_->IsEnabled()) {
    return mojom::MetaKey::kLauncherRefresh;
  } else {
    return mojom::MetaKey::kLauncher;
  }
}

bool KeyboardCapability::UseRefreshedIcons() const {
  return GetMetaKeyToDisplay() == mojom::MetaKey::kLauncherRefresh;
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
  // When `should_disable_trimming_` is true, skip removal of removed devices
  // from our cache of `KeyboardInfo`.
  if (should_disable_trimming_) {
    return;
  }

  auto sorted_keyboards =
      DeviceDataManager::GetInstance()->GetKeyboardDevices();
  base::ranges::sort(sorted_keyboards, [](const ui::KeyboardDevice& device1,
                                          const ui::KeyboardDevice& device2) {
    return device1.id < device2.id;
  });

  // Generate a vector with only the device ids from the
  // `keyboard_info_map_` map. Guaranteed to be sorted as flat_map is always
  // in sorted order by key.
  std::vector<int> cached_keyboard_info_ids;
  cached_keyboard_info_ids.reserve(keyboard_info_map_.size());
  base::ranges::transform(keyboard_info_map_,
                          std::back_inserter(cached_keyboard_info_ids),
                          [](const auto& pair) { return pair.first; });
  DCHECK(base::ranges::is_sorted(cached_keyboard_info_ids));

  // Compares the `cached_keyboard_info_ids` to the id field of
  // `sorted_keyboards`. Ids that are in `cached_keyboard_info_ids` but not
  // in `sorted_keyboards` are inserted into `keyboard_ids_to_remove`.
  // `sorted_keyboards` and `cached_keyboard_info_ids` must be sorted.
  std::vector<int> keyboard_ids_to_remove;
  base::ranges::set_difference(
      cached_keyboard_info_ids, sorted_keyboards,
      std::back_inserter(keyboard_ids_to_remove),
      /*Comp=*/base::ranges::less(),
      /*Proj1=*/std::identity(),
      /*Proj2=*/[](const KeyboardDevice& device) { return device.id; });

  for (const auto& id : keyboard_ids_to_remove) {
    keyboard_info_map_.erase(id);
  }
}

bool KeyboardCapability::HasKeyEvent(const KeyboardCode& key_code,
                                     const KeyboardDevice& keyboard) const {
  // Handle top row keys.
  std::optional<TopRowActionKey> top_row_action_key =
      ConvertToTopRowActionKey(key_code);
  if (top_row_action_key.has_value()) {
    return HasTopRowActionKey(keyboard, top_row_action_key.value());
  }

  // Handle six pack keys.
  if (IsSixPackKey(key_code)) {
    return HasSixPackKey(keyboard);
  }

  // Handle assistant key.
  if (key_code == KeyboardCode::VKEY_ASSISTANT) {
    return HasAssistantKey(keyboard);
  }

  // TODO(zhangwenyu): check other specific keys, e.g. assistant key.
  return true;
}

bool KeyboardCapability::HasKeyEventOnAnyKeyboard(
    const KeyboardCode& key_code) const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasKeyEvent(key_code, keyboard)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::HasTopRowActionKey(const KeyboardDevice& keyboard,
                                            TopRowActionKey action_key) const {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return base::Contains(kLayout1TopRowActionKeys, action_key);
  }

  return base::Contains(keyboard_info->top_row_action_keys, action_key);
}

bool KeyboardCapability::HasTopRowActionKeyOnAnyKeyboard(
    TopRowActionKey action_key) const {
  for (const ui::KeyboardDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasTopRowActionKey(keyboard, action_key)) {
      return true;
    }
  }
  return false;
}

bool KeyboardCapability::IsSplitModifierKeyboard(
    const KeyboardDevice& keyboard) const {
  return HasRightAltKey(keyboard) && HasFunctionKey(keyboard);
}

bool KeyboardCapability::IsChromeOSKeyboard(int device_id) const {
  const auto device_type = GetDeviceType(device_id);
  return device_type == DeviceType::kDeviceInternalKeyboard ||
         device_type == DeviceType::kDeviceExternalChromeOsKeyboard;
}

void KeyboardCapability::SetBoardNameForTesting(const std::string& board_name) {
  board_name_ = board_name;
}

void KeyboardCapability::ForceEnableFeature() {
  modifier_split_dogfood_controller_->ForceEnableFeature();
}

void KeyboardCapability::ResetModifierSplitDogfoodControllerForTesting() {
  modifier_split_dogfood_controller_ =
      std::make_unique<ModifierSplitDogfoodController>();  // IN-TEST
}

}  // namespace ui
