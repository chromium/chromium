// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/touch_device_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace display {

namespace {

ui::TouchscreenDevice CreateTouchscreenDevice(int id,
                                              ui::InputDeviceType type,
                                              const gfx::Size& size) {
  ui::TouchscreenDevice device(id, type, base::NumberToString(id), size, 0);
  device.vendor_id = id * id;
  device.product_id = device.vendor_id * id;
  return device;
}

TouchDeviceManager::TouchAssociationInfo CreateTouchAssociationInfo(
    int days_old) {
  TouchDeviceManager::TouchAssociationInfo info;
  info.timestamp = base::Time::Now() - base::Days(days_old);
  return info;
}

}  // namespace

using DisplayInfoList = std::vector<ManagedDisplayInfo>;

class TouchAssociationTest : public testing::Test {
 public:
  TouchAssociationTest() {}

  TouchAssociationTest(const TouchAssociationTest&) = delete;
  TouchAssociationTest& operator=(const TouchAssociationTest&) = delete;

  ~TouchAssociationTest() override {}

  DisplayManager* display_manager() { return display_manager_.get(); }

  TouchDeviceManager* touch_device_manager() { return touch_device_manager_; }

  // testing::Test:
  void SetUp() override {
    // Recreate for each test, DisplayManager has a lot of state.
    display_manager_ =
        std::make_unique<DisplayManager>(std::make_unique<ScreenBase>());
    touch_device_manager_ = display_manager_->touch_device_manager();

    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    {
      ManagedDisplayInfo display(1, "1", false);
      const ManagedDisplayMode mode(gfx::Size(1920, 1080), 60.0,
                                    false /* interlaced */, true /* native */,
                                    1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }

    {
      ManagedDisplayInfo display(2, "2", false);

      const ManagedDisplayMode mode(gfx::Size(800, 600), 60.0,
                                    false /* interlaced */, true /* native */,
                                    1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }

    // Display without native mode. Must not be matched to any touch screen.
    {
      ManagedDisplayInfo display(3, "3", false);
      displays_.push_back(display);
    }

    {
      ManagedDisplayInfo display(4, "4", false);

      const ManagedDisplayMode mode(
          gfx::Size(1024, 768), 60.0, false /* interlaced */,
          /* native */ true, 1.0 /* device_scale_factor */);
      ManagedDisplayInfo::ManagedDisplayModeList modes(1, mode);
      display.SetManagedDisplayModes(modes);
      displays_.push_back(display);
    }
  }

  void TearDown() override { displays_.clear(); }

  // Helper method to return the count of touch devices associated with the
  // display |info|.
  std::size_t GetTouchDeviceCount(const ManagedDisplayInfo& info) const {
    test::TouchDeviceManagerTestApi tdm_api(touch_device_manager_);
    return tdm_api.GetTouchDeviceCount(info);
  }

  // Helper method that returns true if the display |info| is associated with
  // the touch device |device|.
  bool AreAssociated(const ManagedDisplayInfo& info,
                     const ui::TouchscreenDevice& device) const {
    test::TouchDeviceManagerTestApi tdm_api(touch_device_manager_);
    return tdm_api.AreAssociated(info, device);
  }

 protected:
  DisplayInfoList displays_;
  std::unique_ptr<DisplayManager> display_manager_;
  raw_ptr<TouchDeviceManager> touch_device_manager_;
};

TEST_F(TouchAssociationTest, NoTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  for (size_t i = 0; i < displays_.size(); ++i)
    EXPECT_EQ(GetTouchDeviceCount(displays_[i]), 0u);
}

// Verify that if there are a lot of touchscreens, they will all get associated
// with a display.
TEST_F(TouchAssociationTest, ManyTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;
  for (int i = 0; i < 5; ++i) {
    devices.push_back(CreateTouchscreenDevice(
        i, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(256, 256)));
  }

  DisplayInfoList displays;
  displays.push_back(displays_[3]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  for (int i = 0; i < 5; ++i)
    EXPECT_TRUE(AreAssociated(displays[0], devices[i]));
}

TEST_F(TouchAssociationTest, OneToOneMapping) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(800, 600)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices[0]));
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices[1]));
}

TEST_F(TouchAssociationTest, MapToCorrectDisplaySize) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices[0]));
}

TEST_F(TouchAssociationTest, MapWhenSizeDiffersByOne) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(801, 600)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1023, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices[0]));
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices[1]));
}

TEST_F(TouchAssociationTest, MapWhenSizesDoNotMatch) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1022, 768)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(802, 600)));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  // The touch devices should match to the internal display if they were not
  // matched in any of the steps.
  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices[0]));
  EXPECT_TRUE(AreAssociated(displays_[1], devices[1]));
}

TEST_F(TouchAssociationTest, MapInternalTouchscreen) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(9999, 888)));

  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays, devices);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices[1]));
  EXPECT_TRUE(AreAssociated(displays_[1], devices[0]));
}

TEST_F(TouchAssociationTest, MultipleInternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 2u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 0u);
}

TEST_F(TouchAssociationTest, MultipleInternalAndExternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1024, 768)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices[0]));
  EXPECT_TRUE(AreAssociated(displays_[0], devices[1]));
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices[2]));
}

// crbug.com/515201
TEST_F(TouchAssociationTest, TestWithNoInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(9999, 888)));

  // Internal touchscreen should not be associated with any display
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices[0]));
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 0u);
}

TEST_F(TouchAssociationTest, MatchRemainingDevicesToInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(123, 456)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(234, 567)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(345, 678)));

  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());
  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 3u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices[0]));
  EXPECT_TRUE(AreAssociated(displays_[0], devices[1]));
  EXPECT_TRUE(AreAssociated(displays_[0], devices[2]));
  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);
  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 0u);
}

TEST_F(TouchAssociationTest,
       MatchRemainingDevicesWithNoInternalDisplayPresent) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(CreateTouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(123, 456)));
  devices.push_back(CreateTouchscreenDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(234, 567)));
  devices.push_back(CreateTouchscreenDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(345, 678)));

  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  std::size_t total = 0;
  for (const auto& display : displays_)
    total += GetTouchDeviceCount(display);

  // Make sure all devices were matched.
  EXPECT_EQ(total, devices.size());
}

class TouchAssociationFromPrefTest : public TouchAssociationTest {
 public:
  TouchAssociationFromPrefTest() {}

  TouchAssociationFromPrefTest(const TouchAssociationFromPrefTest&) = delete;
  TouchAssociationFromPrefTest& operator=(const TouchAssociationFromPrefTest&) =
      delete;

  ~TouchAssociationFromPrefTest() override {}

  void SetUp() override {
    TouchAssociationTest::SetUp();
    TouchDeviceManager::TouchAssociationMap touch_associations;

    devices_.push_back(CreateTouchscreenDevice(
        1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));
    devices_.push_back(CreateTouchscreenDevice(
        2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1024, 768)));
    devices_.push_back(CreateTouchscreenDevice(
        3, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(640, 480)));
    devices_.push_back(CreateTouchscreenDevice(
        4, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(800, 600)));

    // Create priority list for Device Id = 1
    //   - Display Index 0
    //   - Display Index 1
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])]
                      [displays_[0].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])]
                      [displays_[1].id()] = CreateTouchAssociationInfo(2);

    // Create priority list for Device Id = 2
    //   - Display Index 3
    //   - Display Index 1
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])]
                      [displays_[3].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])]
                      [displays_[1].id()] = CreateTouchAssociationInfo(2);

    // Create priority list for Device Id = 3
    //   - Display Index 2
    //   - Display Index 3
    //   - Display Index 0
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[2].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[3].id()] = CreateTouchAssociationInfo(2);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[0].id()] = CreateTouchAssociationInfo(3);

    touch_device_manager_->RegisterTouchAssociations(
        touch_associations, TouchDeviceManager::PortAssociationMap());
  }

  void TearDown() override {
    TouchAssociationTest::TearDown();
    devices_.clear();
  }

 protected:
  std::vector<ui::TouchscreenDevice> devices_;
};

TEST_F(TouchAssociationFromPrefTest, CorrectMapping) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
}

TEST_F(TouchAssociationFromPrefTest, CorrectMappingWithSomeMissing) {
  DisplayInfoList displays;
  displays.push_back(displays_[1]);
  displays.push_back(displays_[3]);

  touch_device_manager()->AssociateTouchscreens(&displays, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
}

TEST_F(TouchAssociationFromPrefTest, UpdateMappingBeforeAssociation) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[2], displays_[3].id(), TouchCalibrationData());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
}

TEST_F(TouchAssociationFromPrefTest, UpdateMappingAfterAssociation) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list. This should work even
  // though the association of devices and displays is complete.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[2], displays_[3].id(), TouchCalibrationData());

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
}

TEST_F(TouchAssociationFromPrefTest, AssociatingDeviceToNewDisplay) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[0], displays_[2].id(), TouchCalibrationData());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
}

TEST_F(TouchAssociationFromPrefTest, AssociateDeviceWithNoCalibrationData) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list.
  touch_device_manager()->AddTouchAssociation(devices_[0], displays_[2].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));

  auto calibration_data =
      touch_device_manager()->GetCalibrationData(devices_[0]);
  // Expect it to match the default calibration data (ie no calibration data)
  // since we have not set any.
  EXPECT_EQ(calibration_data, TouchCalibrationData());
}

// Tests that when a touch device is re-associated, the old calibration data is
// not wiped out.
TEST_F(TouchAssociationFromPrefTest,
       AssociateDeviceWithPreviousCalibrationData) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());
  touch_device_manager()->AddTouchCalibrationData(
      devices_[0], displays_[2].id(),
      TouchCalibrationData(/*point_pairs=*/{}, /*bounds=*/gfx::Size(100, 200)));
  {
    auto test_calibration_data =
        touch_device_manager()->GetCalibrationData(devices_[0]);
    EXPECT_EQ(gfx::Size(100, 200), test_calibration_data.bounds);
  }
  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list.
  touch_device_manager()->AddTouchAssociation(devices_[0], displays_[2].id());

  // After updating the assosciation, the calibration data is unchanged since we
  // only changed the association.
  {
    auto test_calibration_data =
        touch_device_manager()->GetCalibrationData(devices_[0]);
    EXPECT_EQ(gfx::Size(100, 200), test_calibration_data.bounds);
  }
}

TEST_F(TouchAssociationFromPrefTest,
       AssociatingDeviceToNewDisplayAfterAssociation) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  // Reassociate display with id 4 to touch device with id 3. This will
  // bring the display to the top of the priority list. This should work even
  // though the association of devices and displays is already complete.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[0], displays_[2].id(), TouchCalibrationData());

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
}

TEST_F(TouchAssociationFromPrefTest, InternalDisplayIsNotMatched) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[0].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
}

class TouchAssociationWithDuplicateDeviceTest : public TouchAssociationTest {
 public:
  TouchAssociationWithDuplicateDeviceTest() {}

  TouchAssociationWithDuplicateDeviceTest(
      const TouchAssociationWithDuplicateDeviceTest&) = delete;
  TouchAssociationWithDuplicateDeviceTest& operator=(
      const TouchAssociationWithDuplicateDeviceTest&) = delete;

  ~TouchAssociationWithDuplicateDeviceTest() override {}

  void SetUp() override {
    TouchAssociationTest::SetUp();
    TouchDeviceManager::TouchAssociationMap touch_associations;
    TouchDeviceManager::PortAssociationMap port_associations;

    // Create different ports.
    const std::vector<std::string> ports = {"port 0", "port 1", "port 2",
                                            "port 3", "port 4"};

    std::string device_name_1 = "device 1";
    std::string device_name_2 = "device 2";

    // Create a device with name |device_name_1| connected to |ports[0]|.
    devices_.push_back(CreateTouchscreenDevice(
        1, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));
    devices_.back().name = device_name_1;
    devices_.back().phys = ports[0];

    int vendor_id = devices_.back().vendor_id;
    int product_id = devices_.back().product_id;

    devices_.push_back(CreateTouchscreenDevice(
        2, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));

    // Create another device with the same name but different port. Ensure that
    // the touch device identifier is the same by setting the same vendor id,
    // product id and name.
    devices_.back().name = device_name_1;
    devices_.back().phys = ports[1];
    devices_.back().vendor_id = vendor_id;
    devices_.back().product_id = product_id;

    devices_.push_back(CreateTouchscreenDevice(
        3, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(1920, 1080)));
    devices_.back().name = device_name_1;
    devices_.back().phys = ports[2];
    devices_.back().vendor_id = vendor_id;
    devices_.back().product_id = product_id;

    devices_.push_back(CreateTouchscreenDevice(
        4, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(800, 600)));

    devices_.push_back(CreateTouchscreenDevice(
        5, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(4096, 4096)));
    devices_.back().name = device_name_2;
    devices_.back().phys = ports[3];

    vendor_id = devices_.back().vendor_id;
    product_id = devices_.back().product_id;

    devices_.push_back(CreateTouchscreenDevice(
        6, ui::InputDeviceType::INPUT_DEVICE_USB, gfx::Size(4096, 4096)));
    devices_.back().name = device_name_2;
    devices_.back().phys = ports[4];
    devices_.back().vendor_id = vendor_id;
    devices_.back().product_id = product_id;

    // Create priority list for Device Id = 1
    //   - Display Index 0
    //   - Display Index 2
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])]
                      [displays_[0].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[0])]
                      [displays_[2].id()] = CreateTouchAssociationInfo(2);

    // Create priority list for Device Id = 2
    //   - Display Index 3
    //   - Display Index 1
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])]
                      [displays_[3].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[1])]
                      [displays_[1].id()] = CreateTouchAssociationInfo(2);

    // Create priority list for Device Id = 3
    //   - Display Index 2
    //   - Display Index 3
    //   - Display Index 0
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[2].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[3].id()] = CreateTouchAssociationInfo(2);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[2])]
                      [displays_[0].id()] = CreateTouchAssociationInfo(3);

    // Create priority list for Device Id = 5
    //   - Display Index 3
    //   - Display Index 2
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[4])] =
        TouchDeviceManager::AssociationInfoMap();
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[4])]
                      [displays_[3].id()] = CreateTouchAssociationInfo(1);
    touch_associations[TouchDeviceIdentifier::FromDevice(devices_[4])]
                      [displays_[2].id()] = CreateTouchAssociationInfo(2);

    // Map ports:
    //   - { Touch Device 1, ports[0] } -> Display Index 2
    //   - { Touch Device 2, ports[1] } -> Display Index 3
    //   - { Touch Device 3, ports[2] } -> Display Index 2
    //   - { Touch Device 5, ports[4] } -> Display Index 0
    port_associations[TouchDeviceIdentifier::FromDevice(devices_[0])] =
        displays_[2].id();
    port_associations[TouchDeviceIdentifier::FromDevice(devices_[1])] =
        displays_[3].id();
    port_associations[TouchDeviceIdentifier::FromDevice(devices_[2])] =
        displays_[2].id();
    port_associations[TouchDeviceIdentifier::FromDevice(devices_[4])] =
        displays_[0].id();

    touch_device_manager_->RegisterTouchAssociations(touch_associations,
                                                     port_associations);
  }

  void TearDown() override {
    TouchAssociationTest::TearDown();
    devices_.clear();
  }

 protected:
  std::vector<ui::TouchscreenDevice> devices_;
};

TEST_F(TouchAssociationWithDuplicateDeviceTest, CorrectMapping) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[4]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[2]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 2u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[5]));
}

TEST_F(TouchAssociationWithDuplicateDeviceTest, NoDuplicateIds) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(devices_[1]);
  devices.push_back(devices_[3]);
  devices.push_back(devices_[4]);

  touch_device_manager()->AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 0u);

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[1]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[4]));
}

TEST_F(TouchAssociationWithDuplicateDeviceTest, CorrectMappingWithSomeMissing) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());
  DisplayInfoList displays;
  displays.push_back(displays_[0]);
  displays.push_back(displays_[1]);
  displays.push_back(displays_[3]);

  touch_device_manager()->AssociateTouchscreens(&displays, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[4]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 4u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[0]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[5]));
}

TEST_F(TouchAssociationWithDuplicateDeviceTest, UpdatePortBeforeAssociation) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  // Reassociate display at index 3 to touch device at index 2. This will
  // bring the display to the top of the priority list and map the port the
  // device is connected to, to display 3.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[2], displays_[3].id(), TouchCalibrationData());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[4]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 3u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[5]));
}

TEST_F(TouchAssociationWithDuplicateDeviceTest, ChangeAssociation) {
  test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                displays_[1].id());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  // Reassociate display at index 3 to touch device at index 2. This will
  // bring the display to the top of the priority list and map the port the
  // device is connected to, to display 3.
  touch_device_manager()->AddTouchCalibrationData(
      devices_[2], displays_[3].id(), TouchCalibrationData());

  touch_device_manager()->AssociateTouchscreens(&displays_, devices_);

  EXPECT_EQ(GetTouchDeviceCount(displays_[0]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[0], devices_[4]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[1]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[1], devices_[3]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[2]), 1u);
  EXPECT_TRUE(AreAssociated(displays_[2], devices_[0]));

  EXPECT_EQ(GetTouchDeviceCount(displays_[3]), 3u);
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[1]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[2]));
  EXPECT_TRUE(AreAssociated(displays_[3], devices_[5]));
}

}  // namespace display
