// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_capability.h"

#include <linux/input-event-codes.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/mojom/meta_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ui {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

constexpr char kKbdTopRowLayoutUnspecified[] = "";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

// A tag that should fail parsing for the top row layout.
constexpr char kKbdTopRowLayoutInvalidTag[] = "X";

// A default example of the layout string read from the function_row_physmap
// sysfs attribute. The values represent the scan codes for each position
// in the top row, which maps to F-Keys.
constexpr char kKbdDefaultCustomTopRowLayout[] =
    "01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f";

// A tag that should fail parsing for the top row custom scan code string.
constexpr char kKbdInvalidCustomTopRowLayout[] = "X X X";

// This set of scan code mappings come from x86 internal vivaldi keyboards.
enum CustomTopRowScanCode : uint32_t {
  kPreviousTrack = 0x90,
  kFullscreen = 0x91,
  kOverview = 0x92,
  kScreenshot = 0x93,
  kScreenBrightnessDown = 0x94,
  kScreenBrightnessUp = 0x95,
  kPrivacyScreenToggle = 0x96,
  kKeyboardBacklightDown = 0x97,
  kKeyboardBacklightUp = 0x98,
  kNextTrack = 0x99,
  kPlayPause = 0x9A,
  kMicrophoneMute = 0x9B,
  kKeyboardBacklightToggle = 0x9E,
  kVolumeMute = 0xA0,
  kVolumeDown = 0xAE,
  kVolumeUp = 0xB0,
  kForward = 0xE9,
  kBack = 0xEA,
  kRefresh = 0xE7,
};

constexpr int kDeviceId1 = 5;
constexpr int kDeviceId2 = 10;

InputDeviceType INTERNAL = InputDeviceType::INPUT_DEVICE_INTERNAL;
InputDeviceType EXTERNAL_USB = InputDeviceType::INPUT_DEVICE_USB;
InputDeviceType EXTERNAL_BLUETOOTH = InputDeviceType::INPUT_DEVICE_BLUETOOTH;
// For INPUT_DEVICE_UNKNOWN type, we treat it as external keyboard.
InputDeviceType EXTERNAL_UNKNOWN = InputDeviceType::INPUT_DEVICE_UNKNOWN;

struct KeyEventTestData {
  // All currently connected keyboards' connection type, e.g.
  // INPUT_DEVICE_INTERNAL.
  std::vector<InputDeviceType> keyboard_connection_types;
  // All currently connected keyboards' layout types.
  std::vector<std::string> keyboard_layout_types;
  KeyboardCode key_code;
  // Expected result of whether this key event exists on each keyboard.
  std::vector<bool> expected_has_key_event;
  // Expected result of whether this key event exists on all connected.
  bool expected_has_key_event_on_any_keyboard;
};

// NOTE: This only creates a simple KeyboardDevice based on a device
// capabilities report; it is not suitable for subclasses of KeyboardDevice.
KeyboardDevice KeyboardDeviceFromCapabilities(
    int device_id,
    const DeviceCapabilities& capabilities) {
  EventDeviceInfo device_info = {};
  CapabilitiesToDeviceInfo(capabilities, &device_info);
  return KeyboardDevice{
      InputDevice(device_id, device_info.device_type(), device_info.name(),
                  device_info.phys(), base::FilePath(capabilities.path),
                  device_info.vendor_id(), device_info.product_id(),
                  device_info.version()),
      device_info.HasKeyEvent(KEY_ASSISTANT), device_info.HasKeyEvent(KEY_FN)};
}

std::optional<uint32_t> GetEvdevKeyCodeForScanCode(const base::ScopedFD& fd,
                                                   uint32_t scancode) {
  switch (scancode) {
    case CustomTopRowScanCode::kPreviousTrack:
      return KEY_PREVIOUSSONG;
    case CustomTopRowScanCode::kFullscreen:
      return KEY_ZOOM;
    case CustomTopRowScanCode::kOverview:
      return KEY_SCALE;
    case CustomTopRowScanCode::kScreenshot:
      return KEY_SYSRQ;
    case CustomTopRowScanCode::kScreenBrightnessDown:
      return KEY_BRIGHTNESSDOWN;
    case CustomTopRowScanCode::kScreenBrightnessUp:
      return KEY_BRIGHTNESSUP;
    case CustomTopRowScanCode::kPrivacyScreenToggle:
      return KEY_PRIVACY_SCREEN_TOGGLE;
    case CustomTopRowScanCode::kKeyboardBacklightDown:
      return KEY_KBDILLUMDOWN;
    case CustomTopRowScanCode::kKeyboardBacklightUp:
      return KEY_KBDILLUMUP;
    case CustomTopRowScanCode::kNextTrack:
      return KEY_NEXTSONG;
    case CustomTopRowScanCode::kPlayPause:
      return KEY_PLAYPAUSE;
    case CustomTopRowScanCode::kMicrophoneMute:
      return KEY_MICMUTE;
    case CustomTopRowScanCode::kKeyboardBacklightToggle:
      return KEY_KBDILLUMTOGGLE;
    case CustomTopRowScanCode::kVolumeMute:
      return KEY_MUTE;
    case CustomTopRowScanCode::kVolumeDown:
      return KEY_VOLUMEDOWN;
    case CustomTopRowScanCode::kVolumeUp:
      return KEY_VOLUMEUP;
    case CustomTopRowScanCode::kForward:
      return KEY_FORWARD;
    case CustomTopRowScanCode::kBack:
      return KEY_BACK;
    case CustomTopRowScanCode::kRefresh:
      return KEY_REFRESH;
  }

  return std::nullopt;
}

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() { RemoveAllDevices(); }

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const KeyboardDevice& fake_keyboard,
                       const std::string& layout,
                       bool has_custom_top_row = false) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    if (has_custom_top_row) {
      sysfs_attributes[kKbdTopRowLayoutAttributeName] = layout;
    } else {
      sysfs_properties[kKbdTopRowPropertyName] = layout;
    }
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

  void RemoveAllDevices() {
    fake_udev_.Reset();
    fake_keyboard_devices_.clear();
    DeviceDataManagerTestApi().SetKeyboardDevices({});
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<KeyboardDevice> fake_keyboard_devices_;
};

}  // namespace

class KeyboardCapabilityTestBase : public testing::Test {
 public:
  KeyboardCapabilityTestBase() = default;
  ~KeyboardCapabilityTestBase() override = default;

  void SetUp() override {
    user_manager_ = std::make_unique<user_manager::FakeUserManager>();
    user_manager_->Initialize();
    keyboard_capability_ = std::make_unique<KeyboardCapability>(
        base::BindRepeating(&GetEvdevKeyCodeForScanCode));
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

  void TearDown() override {
    fake_keyboard_devices_.clear();
    keyboard_capability_.reset();
    user_manager_->Destroy();
    user_manager_.reset();
  }

  KeyboardDevice AddFakeKeyboardInfoToKeyboardCapability(
      int device_id,
      DeviceCapabilities capabilities,
      KeyboardCapability::DeviceType device_type,
      KeyboardCapability::KeyboardTopRowLayout top_row_layout) {
    KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type = device_type;
    keyboard_info.top_row_layout = top_row_layout;

    KeyboardDevice fake_keyboard =
        KeyboardDeviceFromCapabilities(device_id, capabilities);

    keyboard_capability_->SetKeyboardInfoForTesting(fake_keyboard,
                                                    std::move(keyboard_info));
    fake_keyboard_devices_.push_back(fake_keyboard);
    DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);

    return fake_keyboard;
  }

 protected:
  std::unique_ptr<KeyboardCapability> keyboard_capability_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
  std::vector<KeyboardDevice> fake_keyboard_devices_;
};

class KeyboardCapabilityTest : public KeyboardCapabilityTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    modifier_split_feature_list_ =
        std::make_unique<base::test::ScopedFeatureList>();
    if (GetParam()) {
      modifier_split_feature_list_->InitAndEnableFeature(
          ash::features::kModifierSplit);
    } else {
      modifier_split_feature_list_->InitAndDisableFeature(
          ash::features::kModifierSplit);
    }
    KeyboardCapabilityTestBase::SetUp();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> modifier_split_feature_list_;
  base::AutoReset<bool> modifier_split_reset_ =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
};

INSTANTIATE_TEST_SUITE_P(All, KeyboardCapabilityTest, testing::Bool());

TEST_P(KeyboardCapabilityTest, TestIsSixPackKey) {
  for (const auto& [key_code, _] : kSixPackKeyToSearchSystemKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsSixPackKey(key_code));
  }

  for (const auto& [key_code, _] : kSixPackKeyToAltSystemKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsSixPackKey(key_code));
  }

  // A key not in the kSixPackKeyToSystemKeyMap is not a six pack key.
  EXPECT_FALSE(keyboard_capability_->IsSixPackKey(KeyboardCode::VKEY_A));
}

TEST_P(KeyboardCapabilityTest, TestGetMappedFKeyIfExists) {
  KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"fake_Keyboard");
  fake_keyboard.sys_path = base::FilePath("path1");

  // Add a fake layout1 keyboard.
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  for (const auto& [key_code, f_key] : kLayout1TopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_MEDIA_PLAY_PAUSE key is not a top row key for layout1.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
                                           fake_keyboard)
                   .has_value());

  // Replace by a fake layout2 keyboard.
  fake_keyboard_manager_->RemoveAllDevices();
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);
  for (const auto& [key_code, f_key] : kLayout2TopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_BROWSER_FORWARD key is not a top row key for layout2.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(KeyboardCode::VKEY_BROWSER_FORWARD,
                                           fake_keyboard)
                   .has_value());

  // Replace by a fake wilco keyboard.
  fake_keyboard_manager_->RemoveAllDevices();
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  for (const auto& [key_code, f_key] : kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_MEDIA_PLAY_PAUSE key is not a top row key for wilco layout.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
                                           fake_keyboard)
                   .has_value());

  // Replace by a fake drallion keyboard.
  fake_keyboard_manager_->RemoveAllDevices();
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                          kKbdTopRowLayoutDrallionTag);
  for (const auto& [key_code, f_key] : kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_BROWSER_FORWARD key is not a top row key for drallion layout.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(KeyboardCode::VKEY_BROWSER_FORWARD,
                                           fake_keyboard)
                   .has_value());
}

TEST_P(KeyboardCapabilityTest, TestHasLauncherButton) {
  // Add a non-layout2 keyboard.
  KeyboardDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Provide specific keyboard. Launcher button depends on if the keyboard is
  // layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  // Do not provide specific keyboard. Launcher button depends on if any one
  // of the keyboards is layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButtonOnAnyKeyboard());

  // Add a layout2 keyboard.
  KeyboardDevice fake_keyboard2(
      /*id=*/kDeviceId2, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);

  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButtonOnAnyKeyboard());

  fake_keyboard_manager_->RemoveAllDevices();
  // Add an external layout1 keyboard.
  KeyboardDevice fake_keyboard3(
      /*id=*/kDeviceId1, /*type=*/InputDeviceType::INPUT_DEVICE_USB,
      /*name=*/"Keyboard1");
  fake_keyboard3.sys_path = base::FilePath("path3");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard3, kKbdTopRowLayout1Tag);
  EXPECT_TRUE(keyboard_capability_->HasLauncherButtonOnAnyKeyboard());
}

TEST_P(KeyboardCapabilityTest, TestGetMetaKey) {
  // Add a non-layout2 keyboard.
  KeyboardDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Provide specific keyboard. Launcher button depends on if the keyboard is
  // layout2 type.
  EXPECT_EQ(mojom::MetaKey::kSearch,
            keyboard_capability_->GetMetaKey(fake_keyboard1));
  // Do not provide specific keyboard. Launcher button depends on if any one
  // of the keyboards is layout2 type.
  EXPECT_EQ(mojom::MetaKey::kSearch,
            keyboard_capability_->GetMetaKeyToDisplay());

  // Add a layout2 keyboard.
  KeyboardDevice fake_keyboard2(
      /*id=*/kDeviceId2, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);

  EXPECT_EQ(mojom::MetaKey::kSearch,
            keyboard_capability_->GetMetaKey(fake_keyboard1));
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKey(fake_keyboard2));
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());
}

TEST_P(KeyboardCapabilityTest, TestGetMetaKey_ExternalChromeOS) {
  KeyboardDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/InputDeviceType::INPUT_DEVICE_USB,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKey(fake_keyboard1));
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());

  fake_keyboard_manager_->RemoveAllDevices();
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout2Tag);
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKey(fake_keyboard1));
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());
}

TEST_P(KeyboardCapabilityTest, TestGetMetaKey_ExternalNonChromeOS) {
  KeyboardDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/InputDeviceType::INPUT_DEVICE_USB,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_EQ(mojom::MetaKey::kExternalMeta,
            keyboard_capability_->GetMetaKey(fake_keyboard1));
  EXPECT_EQ(ash::features::IsModifierSplitEnabled()
                ? mojom::MetaKey::kLauncherRefresh
                : mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());

  // When an internal keyboard is added, it overrides the meta key from the
  // external keyboard.
  KeyboardDevice internal_keyboard(
      /*id=*/kDeviceId2, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout2Tag);
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKey(internal_keyboard));
  EXPECT_EQ(mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());
}

TEST_P(KeyboardCapabilityTest, TestGetMetaKey_SplitModifierKeyboard) {
  if (!ash::features::IsModifierSplitEnabled()) {
    GTEST_SKIP()
        << "This test is only applicable with split modifier feature enabled.";
  }

  const KeyboardDevice split_modifier_keyboard =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, kSplitModifierKeyboard,
          KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
          KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom);
  EXPECT_EQ(mojom::MetaKey::kLauncherRefresh,
            keyboard_capability_->GetMetaKey(split_modifier_keyboard));
  EXPECT_EQ(mojom::MetaKey::kLauncherRefresh,
            keyboard_capability_->GetMetaKeyToDisplay());
}

TEST_P(KeyboardCapabilityTest, TestGetMetaKey_NoKeyboardsConnected) {
  ASSERT_TRUE(DeviceDataManager::GetInstance()->GetKeyboardDevices().empty());
  EXPECT_EQ(ash::features::IsModifierSplitEnabled()
                ? mojom::MetaKey::kLauncherRefresh
                : mojom::MetaKey::kLauncher,
            keyboard_capability_->GetMetaKeyToDisplay());
}

TEST_P(KeyboardCapabilityTest, TestHasSixPackKey) {
  // Add an internal keyboard.
  KeyboardDevice fake_keyboard1(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Internal keyboard doesn't have six pack key.
  EXPECT_FALSE(keyboard_capability_->HasSixPackKey(fake_keyboard1));
  EXPECT_FALSE(keyboard_capability_->HasSixPackOnAnyKeyboard());

  // Add an external keyboard.
  KeyboardDevice fake_keyboard2(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout1Tag);

  // External keyboard has six pack key.
  EXPECT_TRUE(keyboard_capability_->HasSixPackKey(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasSixPackOnAnyKeyboard());
}

TEST_P(KeyboardCapabilityTest, TestRemoveDevicesFromList) {
  const KeyboardDevice input_device1 = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, kEveKeyboard,
      KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2);
  const KeyboardDevice input_device2 = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId2, kHpUsbKeyboard,
      KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard,
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);

  DeviceDataManagerTestApi().SetKeyboardDevices({input_device1, input_device2});
  ASSERT_EQ(2u, keyboard_capability_->keyboard_info_map().size());

  DeviceDataManagerTestApi().SetKeyboardDevices({input_device1});
  ASSERT_EQ(1u, keyboard_capability_->keyboard_info_map().size());
  EXPECT_TRUE(keyboard_capability_->keyboard_info_map().contains(kDeviceId1));

  DeviceDataManagerTestApi().SetKeyboardDevices({});
  ASSERT_EQ(0u, keyboard_capability_->keyboard_info_map().size());
}

TEST_P(KeyboardCapabilityTest, TestIdentifyRevenKeyboard) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);

  KeyboardDevice internal_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"internal keyboard");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayoutUnspecified);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard,
            keyboard_capability_->GetDeviceType(internal_keyboard));
}

TEST_P(KeyboardCapabilityTest, TestIsTopRowKey) {
  for (const auto& [key_code, _] : kLayout1TopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }
  for (const auto& [key_code, _] : kLayout2TopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }
  for (const auto& [key_code, _] : kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }

  // A key not in any of the above maps is not a top row key.
  EXPECT_FALSE(keyboard_capability_->IsTopRowKey(KeyboardCode::VKEY_A));
}

TEST_P(KeyboardCapabilityTest, TestHasGlobeKey) {
  KeyboardDevice bluetooth_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard1");
  bluetooth_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(bluetooth_keyboard));

  fake_keyboard_manager_->RemoveAllDevices();
  KeyboardDevice internal_keyboard_layout(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  internal_keyboard_layout.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_layout,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasGlobeKey(internal_keyboard_layout));

  KeyboardDevice bluetooth_keyboard_layout1(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  bluetooth_keyboard_layout1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard_layout1,
                                          kKbdTopRowLayout1Tag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(bluetooth_keyboard_layout1));

  KeyboardDevice bluetooth_keyboard_layout2(
      /*id=*/3, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard3");
  bluetooth_keyboard_layout2.sys_path = base::FilePath("path3");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard_layout2,
                                          kKbdTopRowLayout2Tag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(bluetooth_keyboard_layout2));

  KeyboardDevice bluetooth_keyboard_layout_custom(
      /*id=*/4, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard4");
  bluetooth_keyboard_layout_custom.sys_path = base::FilePath("path4");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard_layout_custom,
                                          kKbdDefaultCustomTopRowLayout,
                                          /*has_custom_top_row=*/true);
  EXPECT_TRUE(
      keyboard_capability_->HasGlobeKey(bluetooth_keyboard_layout_custom));

  KeyboardDevice bluetooth_keyboard_wilco(
      /*id=*/5, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard5");
  bluetooth_keyboard_wilco.sys_path = base::FilePath("path5");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard_wilco,
                                          kKbdTopRowLayoutWilcoTag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(bluetooth_keyboard_wilco));

  KeyboardDevice bluetooth_keyboard_drallion(
      /*id=*/6, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard6");
  bluetooth_keyboard_drallion.sys_path = base::FilePath("path6");
  fake_keyboard_manager_->AddFakeKeyboard(bluetooth_keyboard_drallion,
                                          kKbdTopRowLayoutDrallionTag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(bluetooth_keyboard_drallion));
}

TEST_P(KeyboardCapabilityTest, TestHasCalculatorKey) {
  KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasCalculatorKey(internal_keyboard));

  KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasCalculatorKey(external_keyboard));
}

TEST_P(KeyboardCapabilityTest, TestHasBrowserSearchKey) {
  KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasBrowserSearchKey(internal_keyboard));

  KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasBrowserSearchKey(external_keyboard));
}

TEST_P(KeyboardCapabilityTest, TestHasMediaKeys) {
  KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasMediaKeys(internal_keyboard));

  KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasMediaKeys(external_keyboard));
}

TEST_P(KeyboardCapabilityTest, TestHasHelpKey) {
  KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasHelpKey(internal_keyboard));

  KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasHelpKey(external_keyboard));
}

TEST_P(KeyboardCapabilityTest, TestHasSettingsKey) {
  KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasSettingsKey(internal_keyboard));

  KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasSettingsKey(external_keyboard));
}

class ModifierKeyTest : public KeyboardCapabilityTestBase,
                        public testing::WithParamInterface<
                            std::tuple<DeviceCapabilities,
                                       KeyboardCapability::DeviceType,
                                       KeyboardCapability::KeyboardTopRowLayout,
                                       std::vector<mojom::ModifierKey>>> {
 protected:
  base::AutoReset<bool> modifier_split_reset_ =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
};

// Tests that the given `DeviceCapabilities` and
// `KeyboardCapability::DeviceType` combo generates the given set of
// modifier keys.
INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierKeyTest,
    testing::ValuesIn(std::vector<
                      std::tuple<DeviceCapabilities,
                                 KeyboardCapability::DeviceType,
                                 KeyboardCapability::KeyboardTopRowLayout,
                                 std::vector<mojom::ModifierKey>>>{
        {kDrobitKeyboard,
         KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
         KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom,
         {mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
          mojom::ModifierKey::kMeta, mojom::ModifierKey::kEscape,
          mojom::ModifierKey::kAlt}},
        {kLogitechKeyboardK120,
         KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard,
         KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1,
         {mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
          mojom::ModifierKey::kMeta, mojom::ModifierKey::kEscape,
          mojom::ModifierKey::kAlt, mojom::ModifierKey::kCapsLock}},
        {kHpUsbKeyboard,
         KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard,
         KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1,
         {mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
          mojom::ModifierKey::kMeta, mojom::ModifierKey::kEscape,
          mojom::ModifierKey::kAlt, mojom::ModifierKey::kCapsLock}},
        // Tests that an external chromeos keyboard correctly omits capslock.
        {kHpUsbKeyboard,
         KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
         KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom,
         {mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
          mojom::ModifierKey::kMeta, mojom::ModifierKey::kEscape,
          mojom::ModifierKey::kAlt}}}));

TEST_P(ModifierKeyTest, TestGetModifierKeys) {
  auto [capabilities, device_type, top_row_layout, expected_modifier_keys] =
      GetParam();

  const KeyboardDevice test_keyboard = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, capabilities, device_type, top_row_layout);
  auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);

  base::ranges::sort(expected_modifier_keys);
  base::ranges::sort(modifier_keys);
  EXPECT_EQ(expected_modifier_keys, modifier_keys);
}

TEST_P(KeyboardCapabilityTest, TestGetModifierKeysForSplitModifierKeyboard) {
  if (!ash::features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with Modifier Split flag enabled.";
  }

  const KeyboardDevice test_keyboard = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, kSplitModifierKeyboard,
      KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom);
  auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);

  std::vector<mojom::ModifierKey> expected_modifier_keys = {
      mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
      mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
      mojom::ModifierKey::kAlt,       mojom::ModifierKey::kFunction,
      mojom::ModifierKey::kRightAlt};
  base::ranges::sort(expected_modifier_keys);
  base::ranges::sort(modifier_keys);
  EXPECT_EQ(expected_modifier_keys, modifier_keys);
}

class KeyboardCapabilityDogfoodTest : public KeyboardCapabilityTestBase {
 public:
  void SetUp() override {
    modifier_split_feature_list_ =
        std::make_unique<base::test::ScopedFeatureList>();
    modifier_split_feature_list_->InitWithFeatures(
        {ash::features::kModifierSplit, ash::features::kModifierSplitDogfood},
        {});
    KeyboardCapabilityTestBase::SetUp();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> modifier_split_feature_list_;
};

// With the dogfood flag enabled AND no Google account logged in, the feature
// should act as though its disabled.
TEST_F(KeyboardCapabilityDogfoodTest,
       DISABLED_TestGetModifierKeysForSplitModifierKeyboardDogfood) {
  AccountId non_google_account_id =
      AccountId::FromUserEmail("testaccount@gmail.com");
  AccountId google_account_id =
      AccountId::FromUserEmail("testaccount@google.com");
  user_manager_->AddUser(non_google_account_id);
  user_manager_->AddUser(google_account_id);

  // When a non-google account is signed in, keyboard capability should not
  // consider it a split modifier keyboard.
  user_manager_->UserLoggedIn(
      non_google_account_id,
      user_manager::FakeUserManager::GetFakeUsernameHash(non_google_account_id),
      /*browser_restart=*/false, /*is_child=*/false);
  const KeyboardDevice test_keyboard = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, kSplitModifierKeyboard,
      KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom);
  {
    auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);

    std::vector<mojom::ModifierKey> expected_modifier_keys = {
        mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
        mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
        mojom::ModifierKey::kAlt,       mojom::ModifierKey::kAssistant};
    base::ranges::sort(expected_modifier_keys);
    base::ranges::sort(modifier_keys);
    EXPECT_EQ(expected_modifier_keys, modifier_keys);
  }
  user_manager_->LogoutAllUsers();

  // Once a google account signs in, it should now be considered a split
  // modifier keyboard.
  user_manager_->UserLoggedIn(
      google_account_id,
      user_manager::FakeUserManager::GetFakeUsernameHash(google_account_id),
      /*browser_restart=*/false, /*is_child=*/false);
  {
    auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);
    std::vector<mojom::ModifierKey> expected_modifier_keys = {
        mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
        mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
        mojom::ModifierKey::kAlt,       mojom::ModifierKey::kFunction,
        mojom::ModifierKey::kRightAlt};
    base::ranges::sort(expected_modifier_keys);
    base::ranges::sort(modifier_keys);
    EXPECT_EQ(expected_modifier_keys, modifier_keys);
  }
}

TEST_P(KeyboardCapabilityTest, TestGetModifierKeysForEveKeyboard) {
  keyboard_capability_->SetBoardNameForTesting("eve");

  const KeyboardDevice test_keyboard = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, kEveKeyboard,
      KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2);
  auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);

  std::vector<mojom::ModifierKey> expected_modifier_keys = {
      mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
      mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
      mojom::ModifierKey::kAlt,       mojom::ModifierKey::kAssistant};
  base::ranges::sort(expected_modifier_keys);
  base::ranges::sort(modifier_keys);
  EXPECT_EQ(expected_modifier_keys, modifier_keys);
}

class KeyEventTest
    : public KeyboardCapabilityTestBase,
      public testing::WithParamInterface<std::tuple<bool, KeyEventTestData>> {
 public:
  void SetUp() override {
    modifier_split_feature_list_ =
        std::make_unique<base::test::ScopedFeatureList>();
    if (std::get<0>(GetParam())) {
      modifier_split_feature_list_->InitAndEnableFeature(
          ash::features::kModifierSplit);
    } else {
      modifier_split_feature_list_->InitAndDisableFeature(
          ash::features::kModifierSplit);
    }
    KeyboardCapabilityTestBase::SetUp();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> modifier_split_feature_list_;
  base::AutoReset<bool> modifier_split_reset_ =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
};

// Tests that given the keyboard connection type and layout type, check if this
// keyboard has a specific key event.
INSTANTIATE_TEST_SUITE_P(
    All,
    KeyEventTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(std::vector<KeyEventTestData>{
            // Testing top row keys.
            {{INTERNAL},
             {kKbdTopRowLayout1Tag},
             VKEY_BROWSER_FORWARD,
             {true},
             true},
            {{EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout1Tag},
             VKEY_ZOOM,
             {true},
             true},
            {{EXTERNAL_USB},
             {kKbdTopRowLayout1Tag},
             VKEY_MEDIA_PLAY_PAUSE,
             {false},
             false},
            {{INTERNAL},
             {kKbdTopRowLayout2Tag},
             VKEY_BROWSER_FORWARD,
             {false},
             false},
            {{EXTERNAL_UNKNOWN},
             {kKbdTopRowLayout2Tag},
             VKEY_MEDIA_PLAY_PAUSE,
             {true},
             true},
            {{INTERNAL}, {kKbdTopRowLayoutWilcoTag}, VKEY_ZOOM, {true}, true},
            {{EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayoutDrallionTag},
             VKEY_BRIGHTNESS_UP,

             {true},
             true},
            {{INTERNAL, EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
             VKEY_BROWSER_FORWARD,
             {true, false},
             true},
            {{INTERNAL, EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout2Tag, kKbdTopRowLayout2Tag},
             VKEY_BROWSER_FORWARD,
             {false, false},
             false},
            {{INTERNAL, EXTERNAL_USB, EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag,
              kKbdTopRowLayoutWilcoTag},
             VKEY_VOLUME_UP,
             {true, true, true},
             true},

            // Testing six pack keys.
            {{INTERNAL}, {kKbdTopRowLayout1Tag}, VKEY_INSERT, {false}, false},
            {{EXTERNAL_USB}, {kKbdTopRowLayout1Tag}, VKEY_INSERT, {true}, true},
            {{INTERNAL, EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout1Tag, kKbdTopRowLayoutWilcoTag},
             VKEY_HOME,
             {false, true},
             true},

            // Testing other keys.
            {{INTERNAL}, {kKbdTopRowLayout1Tag}, VKEY_LEFT, {true}, true},
            {{EXTERNAL_BLUETOOTH},
             {kKbdTopRowLayout2Tag},
             VKEY_ESCAPE,
             {true},
             true},
            {{EXTERNAL_UNKNOWN},
             {kKbdTopRowLayoutWilcoTag},
             VKEY_A,
             {true},
             true},
            {{INTERNAL}, {kKbdTopRowLayoutDrallionTag}, VKEY_2, {true}, true},
        })));

TEST_P(KeyEventTest, TestHasKeyEvent) {
  auto [keyboard_connection_types, keyboard_layout_types, key_code,
        expected_has_key_event, expected_has_key_event_on_any_keyboard] =
      std::get<1>(GetParam());

  fake_keyboard_manager_->RemoveAllDevices();
  for (size_t i = 0; i < keyboard_layout_types.size(); i++) {
    std::string layout = keyboard_layout_types[i];
    KeyboardDevice fake_keyboard(
        /*id=*/i, /*type=*/keyboard_connection_types[i],
        /*name=*/layout);
    fake_keyboard.sys_path = base::FilePath("path" + layout);
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, layout);

    if (expected_has_key_event[i]) {
      EXPECT_TRUE(keyboard_capability_->HasKeyEvent(key_code, fake_keyboard));
    } else {
      EXPECT_FALSE(keyboard_capability_->HasKeyEvent(key_code, fake_keyboard));
    }
  }

  if (expected_has_key_event_on_any_keyboard) {
    EXPECT_TRUE(keyboard_capability_->HasKeyEventOnAnyKeyboard(key_code));
  } else {
    EXPECT_FALSE(keyboard_capability_->HasKeyEventOnAnyKeyboard(key_code));
  }
}

TEST_P(KeyboardCapabilityTest, TestHasAssistantKey) {
  // Add a fake kEveKeyboard keyboard, which has the assistant key.
  const KeyboardDevice test_keyboard_1 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, kEveKeyboard,
          KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
          KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2);

  keyboard_capability_->SetBoardNameForTesting("eve");
  EXPECT_TRUE(keyboard_capability_->HasAssistantKey(test_keyboard_1));

  keyboard_capability_->SetBoardNameForTesting("nocturne");
  EXPECT_TRUE(keyboard_capability_->HasAssistantKey(test_keyboard_1));

  keyboard_capability_->SetBoardNameForTesting("atlas");
  EXPECT_TRUE(keyboard_capability_->HasAssistantKey(test_keyboard_1));

  keyboard_capability_->SetBoardNameForTesting("anything_else");
  EXPECT_EQ(!GetParam(),
            keyboard_capability_->HasAssistantKey(test_keyboard_1));

  // Reset board back to eve to test that device identification works as
  // expected in the false case.
  keyboard_capability_->SetBoardNameForTesting("eve");

  // Add a fake kDrallionKeyboard keyboard, which does not have the
  // assistant key.
  const KeyboardDevice test_keyboard_2 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, kDrallionKeyboard,
          KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
          KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion);

  EXPECT_FALSE(keyboard_capability_->HasAssistantKey(test_keyboard_2));
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardUnspecified) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutUnspecified);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardInvalidLayoutTag) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutInvalidTag);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceUnknown,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_TRUE(!keyboard_capability_->GetTopRowScanCodes(input_device) ||
              keyboard_capability_->GetTopRowScanCodes(input_device)->empty());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardInvalidCustomLayout) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(
      input_device, kKbdInvalidCustomTopRowLayout, /*has_custom_top_row=*/true);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardLayout1External) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_UNKNOWN,
                              "External Chrome Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout1Tag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardLayout2External) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_UNKNOWN,
                              "External Chrome Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout2Tag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardCustomLayout) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Custom Layout Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdDefaultCustomTopRowLayout,
                                          /*has_custom_top_row=*/true);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom,
            keyboard_capability_->GetTopRowLayout(input_device));

  const auto* top_row_scan_codes_ptr =
      keyboard_capability_->GetTopRowScanCodes(input_device);
  ASSERT_TRUE(top_row_scan_codes_ptr);
  const auto& top_row_scan_codes = *top_row_scan_codes_ptr;

  // Basic inspection to match kKbdDefaultCustomTopRowLayout
  EXPECT_EQ(15u, top_row_scan_codes.size());

  for (size_t i = 0; i < top_row_scan_codes.size(); i++) {
    EXPECT_EQ(i + 1, top_row_scan_codes[i]);
  }
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardWilcoTopRowLayout) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutWilcoTag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, IdentifyKeyboardDrallionTopRowLayout) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutDrallionTag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_P(KeyboardCapabilityTest, TopRowLayout1) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout1Tag,
                                          /*has_custom_top_row=*/false);

  for (TopRowActionKey action_key = TopRowActionKey::kNone;
       action_key <= TopRowActionKey::kMaxValue;
       action_key =
           static_cast<TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        base::Contains(kLayout1TopRowActionKeys, action_key),
        keyboard_capability_->HasTopRowActionKey(input_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  KeyboardCode expected_fkey = VKEY_F1;
  for (const auto action_key : kLayout1TopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 input_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  input_device, expected_fkey));
    expected_fkey =
        static_cast<KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

TEST_P(KeyboardCapabilityTest, TopRowLayout2) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout2Tag,
                                          /*has_custom_top_row=*/false);

  for (TopRowActionKey action_key = TopRowActionKey::kNone;
       action_key <= TopRowActionKey::kMaxValue;
       action_key =
           static_cast<TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        base::Contains(kLayout2TopRowActionKeys, action_key),
        keyboard_capability_->HasTopRowActionKey(input_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  KeyboardCode expected_fkey = VKEY_F1;
  for (const auto action_key : kLayout2TopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 input_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  input_device, expected_fkey));
    expected_fkey =
        static_cast<KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

TEST_P(KeyboardCapabilityTest, TopRowLayoutWilco) {
  KeyboardDevice wilco_device(kDeviceId1, INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_device,
                                          kKbdTopRowLayoutWilcoTag,
                                          /*has_custom_top_row=*/false);
  KeyboardDevice drallion_device(kDeviceId2, INPUT_DEVICE_INTERNAL,
                                 "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(drallion_device,
                                          kKbdTopRowLayoutDrallionTag,
                                          /*has_custom_top_row=*/false);

  for (TopRowActionKey action_key = TopRowActionKey::kNone;
       action_key <= TopRowActionKey::kMaxValue;
       action_key =
           static_cast<TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        base::Contains(kLayoutWilcoDrallionTopRowActionKeys, action_key),
        keyboard_capability_->HasTopRowActionKey(wilco_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
    EXPECT_EQ(
        base::Contains(kLayoutWilcoDrallionTopRowActionKeys, action_key),
        keyboard_capability_->HasTopRowActionKey(drallion_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  KeyboardCode expected_fkey = VKEY_F1;
  for (const auto action_key : kLayoutWilcoDrallionTopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 wilco_device, action_key));
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 drallion_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  wilco_device, expected_fkey));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  drallion_device, expected_fkey));
    expected_fkey =
        static_cast<KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

TEST_P(KeyboardCapabilityTest, NullTopRowDescriptor) {
  KeyboardDevice input_device(kDeviceId1, INPUT_DEVICE_BLUETOOTH,
                              "External Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          "C0000 C0000 C0000 C0000",
                                          /*has_custom_top_row=*/true);
  EXPECT_EQ(
      KeyboardCapability::DeviceType::kDeviceExternalNullTopRowChromeOsKeyboard,
      keyboard_capability_->GetDeviceType(input_device));
  EXPECT_TRUE(keyboard_capability_->HasCapsLockKey(input_device));
}

class TopRowLayoutCustomTest
    : public KeyboardCapabilityTestBase,
      public testing::WithParamInterface<std::vector<TopRowActionKey>> {
 public:
  void SetUp() override {
    KeyboardCapabilityTestBase::SetUp();
    top_row_action_keys_ = GetParam();
    custom_layout_string_.clear();

    std::vector<std::string> custom_scan_codes;
    custom_scan_codes.reserve(top_row_action_keys_.size());
    for (const auto& action_key : top_row_action_keys_) {
      const uint32_t scan_code = ConvertTopRowActionKeyToScanCode(action_key);
      custom_scan_codes.push_back(
          base::ToLowerASCII(base::HexEncode(&scan_code, 1)));
    }

    custom_layout_string_ = base::JoinString(custom_scan_codes, " ");
  }

  uint32_t ConvertTopRowActionKeyToScanCode(TopRowActionKey action_key) {
    switch (action_key) {
      case TopRowActionKey::kBack:
        return CustomTopRowScanCode::kBack;
      case TopRowActionKey::kForward:
        return CustomTopRowScanCode::kForward;
      case TopRowActionKey::kRefresh:
        return CustomTopRowScanCode::kRefresh;
      case TopRowActionKey::kFullscreen:
        return CustomTopRowScanCode::kFullscreen;
      case TopRowActionKey::kOverview:
        return CustomTopRowScanCode::kOverview;
      case TopRowActionKey::kScreenshot:
        return CustomTopRowScanCode::kScreenshot;
      case TopRowActionKey::kScreenBrightnessDown:
        return CustomTopRowScanCode::kScreenBrightnessDown;
      case TopRowActionKey::kScreenBrightnessUp:
        return CustomTopRowScanCode::kScreenBrightnessUp;
      case TopRowActionKey::kMicrophoneMute:
        return CustomTopRowScanCode::kMicrophoneMute;
      case TopRowActionKey::kVolumeMute:
        return CustomTopRowScanCode::kVolumeMute;
      case TopRowActionKey::kVolumeDown:
        return CustomTopRowScanCode::kVolumeDown;
      case TopRowActionKey::kVolumeUp:
        return CustomTopRowScanCode::kVolumeUp;
      case TopRowActionKey::kKeyboardBacklightToggle:
        return CustomTopRowScanCode::kKeyboardBacklightToggle;
      case TopRowActionKey::kKeyboardBacklightDown:
        return CustomTopRowScanCode::kKeyboardBacklightDown;
      case TopRowActionKey::kKeyboardBacklightUp:
        return CustomTopRowScanCode::kKeyboardBacklightUp;
      case TopRowActionKey::kNextTrack:
        return CustomTopRowScanCode::kNextTrack;
      case TopRowActionKey::kPreviousTrack:
        return CustomTopRowScanCode::kPreviousTrack;
      case TopRowActionKey::kPlayPause:
        return CustomTopRowScanCode::kPlayPause;
      case TopRowActionKey::kPrivacyScreenToggle:
        return CustomTopRowScanCode::kPrivacyScreenToggle;
      case TopRowActionKey::kAccessibility:
      case TopRowActionKey::kAllApplications:
      case TopRowActionKey::kEmojiPicker:
      case TopRowActionKey::kDictation:
      case TopRowActionKey::kUnknown:
      case TopRowActionKey::kNone:
        return 0;
    }
  }

 protected:
  std::vector<TopRowActionKey> top_row_action_keys_;
  std::string custom_layout_string_;
  base::AutoReset<bool> modifier_split_reset_ =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
};

INSTANTIATE_TEST_SUITE_P(
    All,
    TopRowLayoutCustomTest,
    testing::ValuesIn(std::vector<std::vector<TopRowActionKey>>{
        // Test with full 15 key set.
        {
            TopRowActionKey::kBack,
            TopRowActionKey::kForward,
            TopRowActionKey::kRefresh,
            TopRowActionKey::kFullscreen,
            TopRowActionKey::kOverview,
            TopRowActionKey::kScreenshot,
            TopRowActionKey::kScreenBrightnessDown,
            TopRowActionKey::kScreenBrightnessUp,
            TopRowActionKey::kMicrophoneMute,
            TopRowActionKey::kVolumeMute,
            TopRowActionKey::kVolumeDown,
            TopRowActionKey::kVolumeUp,
            TopRowActionKey::kKeyboardBacklightToggle,
            TopRowActionKey::kKeyboardBacklightDown,
            TopRowActionKey::kKeyboardBacklightUp,
        },
        // Test the remaining untested set of keys.
        {
            TopRowActionKey::kOverview,
            TopRowActionKey::kScreenshot,
            TopRowActionKey::kScreenBrightnessDown,
            TopRowActionKey::kScreenBrightnessUp,
            TopRowActionKey::kMicrophoneMute,
            TopRowActionKey::kVolumeMute,
            TopRowActionKey::kVolumeDown,
            TopRowActionKey::kVolumeUp,
            TopRowActionKey::kKeyboardBacklightToggle,
            TopRowActionKey::kKeyboardBacklightDown,
            TopRowActionKey::kKeyboardBacklightUp,
            TopRowActionKey::kNextTrack,
            TopRowActionKey::kPreviousTrack,
            TopRowActionKey::kPlayPause,
        },
        // Tests with a small subset of the possible keys.
        {
            TopRowActionKey::kBack,
            TopRowActionKey::kForward,
            TopRowActionKey::kRefresh,
            TopRowActionKey::kPrivacyScreenToggle,
        },
        {
            TopRowActionKey::kMicrophoneMute,
            TopRowActionKey::kVolumeMute,
            TopRowActionKey::kVolumeDown,
            TopRowActionKey::kVolumeUp,
            TopRowActionKey::kKeyboardBacklightToggle,
        }}));

TEST_P(TopRowLayoutCustomTest, TopRowLayout) {
  KeyboardDevice keyboard(kDeviceId1, INPUT_DEVICE_INTERNAL,
                          "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(keyboard, custom_layout_string_,
                                          /*has_custom_top_row=*/true);
  for (TopRowActionKey action_key = TopRowActionKey::kNone;
       action_key <= TopRowActionKey::kMaxValue;
       action_key =
           static_cast<TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(base::Contains(top_row_action_keys_, action_key),
              keyboard_capability_->HasTopRowActionKey(keyboard, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  KeyboardCode expected_fkey = VKEY_F1;
  for (const auto action_key : top_row_action_keys_) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 keyboard, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  keyboard, expected_fkey));
    expected_fkey =
        static_cast<KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

}  // namespace ui
