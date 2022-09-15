// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_data_manager.h"

#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

class DeviceDataManagerTest : public testing::Test {
 public:
  DeviceDataManagerTest() {}

  DeviceDataManagerTest(const DeviceDataManagerTest&) = delete;
  DeviceDataManagerTest& operator=(const DeviceDataManagerTest&) = delete;

  ~DeviceDataManagerTest() override {}

  // testing::Test:
  void SetUp() override { DeviceDataManager::CreateInstance(); }
  void TearDown() override { DeviceDataManager::DeleteInstance(); }

 protected:
  void CallOnDeviceListsComplete() {
    DeviceDataManager::GetInstance()->OnDeviceListsComplete();
  }
};

TEST_F(DeviceDataManagerTest, DisplayIdUpdated) {
  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  std::vector<TouchscreenDevice> touchscreen_devices(1);
  // Default id is invalid, need something other than 0 (0 is invalid).
  constexpr int kTouchId = 1;
  touchscreen_devices[0].id = kTouchId;
  static_cast<DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchscreenDevicesUpdated(touchscreen_devices);
  ASSERT_EQ(1u, device_data_manager->GetTouchscreenDevices().size());
  EXPECT_EQ(display::kInvalidDisplayId,
            device_data_manager->GetTouchscreenDevices()[0].target_display_id);

  constexpr int64_t kDisplayId = 2;
  std::vector<TouchDeviceTransform> touch_device_transforms(1);
  touch_device_transforms[0].display_id = kDisplayId;
  touch_device_transforms[0].device_id = kTouchId;
  device_data_manager->ConfigureTouchDevices(touch_device_transforms);
  ASSERT_EQ(1u, device_data_manager->GetTouchscreenDevices().size());
  EXPECT_EQ(kDisplayId,
            device_data_manager->GetTouchscreenDevices()[0].target_display_id);
}

namespace {

class TestInputDeviceEventObserver : public InputDeviceEventObserver {
 public:
  TestInputDeviceEventObserver() = default;

  TestInputDeviceEventObserver(const TestInputDeviceEventObserver&) = delete;
  TestInputDeviceEventObserver& operator=(const TestInputDeviceEventObserver&) =
      delete;

  int on_touch_device_associations_changed_call_count() const {
    return on_touch_device_associations_changed_call_count_;
  }

  // InputDeviceEventObserver:
  void OnTouchDeviceAssociationChanged() override {
    on_touch_device_associations_changed_call_count_++;
  }

 private:
  int on_touch_device_associations_changed_call_count_ = 0;
};

}  // namespace

TEST_F(DeviceDataManagerTest, AreTouchscreenTargetDisplaysValid) {
  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  EXPECT_FALSE(device_data_manager->AreTouchscreenTargetDisplaysValid());
  TestInputDeviceEventObserver observer;
  base::ScopedObservation<DeviceDataManager, InputDeviceEventObserver>
      scoped_obaservation(&observer);
  scoped_obaservation.Observe(device_data_manager);
  CallOnDeviceListsComplete();
  EXPECT_FALSE(device_data_manager->AreTouchscreenTargetDisplaysValid());
  EXPECT_EQ(0, observer.on_touch_device_associations_changed_call_count());

  device_data_manager->ConfigureTouchDevices({});
  EXPECT_EQ(1, observer.on_touch_device_associations_changed_call_count());
  EXPECT_TRUE(device_data_manager->AreTouchscreenTargetDisplaysValid());

  std::vector<TouchscreenDevice> touchscreen_devices(1);
  // Default id is invalid, need something other than 0 (0 is invalid).
  constexpr int kTouchId = 1;
  touchscreen_devices[0].id = kTouchId;
  static_cast<DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchscreenDevicesUpdated(touchscreen_devices);
  EXPECT_EQ(1, observer.on_touch_device_associations_changed_call_count());
  EXPECT_FALSE(device_data_manager->AreTouchscreenTargetDisplaysValid());
  device_data_manager->ConfigureTouchDevices({});
  EXPECT_EQ(2, observer.on_touch_device_associations_changed_call_count());
  EXPECT_TRUE(device_data_manager->AreTouchscreenTargetDisplaysValid());
}

}  // namespace ui
