// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_data_manager_x11.h"

#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {
namespace test {
namespace {

class TestInputDeviceObserver : public InputDeviceEventObserver {
 public:
  explicit TestInputDeviceObserver(DeviceDataManagerX11* manager)
      : manager_(manager), change_notified_(false) {
    if (manager_)
      manager_->AddObserver(this);
  }

  TestInputDeviceObserver(const TestInputDeviceObserver&) = delete;
  TestInputDeviceObserver& operator=(const TestInputDeviceObserver&) = delete;

  ~TestInputDeviceObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  // InputDeviceEventObserver implementation.
  void OnInputDeviceConfigurationChanged(uint8_t) override {
    change_notified_ = true;
  }

  int change_notified() const { return change_notified_; }
  void Reset() { change_notified_ = false; }

 private:
  raw_ptr<DeviceDataManager> manager_;
  bool change_notified_;
};

}  //  namespace

class DeviceDataManagerX11Test : public testing::Test {
 public:
  DeviceDataManagerX11Test() = default;

  DeviceDataManagerX11Test(const DeviceDataManagerX11Test&) = delete;
  DeviceDataManagerX11Test& operator=(const DeviceDataManagerX11Test&) = delete;

  ~DeviceDataManagerX11Test() override = default;

  void SetUp() override { DeviceDataManagerX11::CreateInstance(); }

  void TearDown() override {
    SetKeyboardDevices(std::vector<KeyboardDevice>());
  }

  virtual void SetKeyboardDevices(const std::vector<KeyboardDevice>& devices) {
    DeviceHotplugEventObserver* manager = DeviceDataManagerX11::GetInstance();
    manager->OnKeyboardDevicesUpdated(devices);
  }

  void SetUpScrollDevice(x11::Input::DeviceId device_id,
                         uint16_t horizontal_number,
                         int32_t horizontal_increment,
                         int32_t horizontal_value,
                         uint16_t vertical_number,
                         int32_t vertical_increment,
                         int32_t vertical_value) {
    DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
    manager->SetDeviceListForTest({}, {}, {static_cast<int>(device_id)});
    manager->UpdateScrollClassDevice(
        x11::Input::DeviceClass::Scroll{
            .number = horizontal_number,
            .scroll_type = x11::Input::ScrollType::Horizontal,
            .increment = {horizontal_increment, 0},
        },
        device_id);
    manager->UpdateScrollClassDevice(
        x11::Input::DeviceClass::Scroll{
            .number = vertical_number,
            .scroll_type = x11::Input::ScrollType::Vertical,
            .increment = {vertical_increment, 0},
        },
        device_id);
    manager->UpdateScrollClassValuators(
        device_id, {
                       x11::Input::DeviceClass{
                           .valuator =
                               x11::Input::DeviceClass::Valuator{
                                   .number = horizontal_number,
                                   .value = {horizontal_value, 0},
                               },
                       },
                       x11::Input::DeviceClass{
                           .valuator =
                               x11::Input::DeviceClass::Valuator{
                                   .number = vertical_number,
                                   .value = {vertical_value, 0},
                               },
                       },
                   });
  }

  x11::Event CreateScrollMotionEvent(x11::Input::DeviceId device_id,
                                     uint16_t valuator_index,
                                     int32_t valuator_value) {
    return x11::Event(false, x11::Input::DeviceEvent{
                                 .opcode = x11::Input::DeviceEvent::Motion,
                                 .deviceid = device_id,
                                 .sourceid = device_id,
                                 .valuator_mask = {1u << valuator_index},
                                 .axisvalues = {{valuator_value, 0}},
                             });
  }
};

// Tests that the the device data manager notifies observers when a device is
// disabled and re-enabled.
TEST_F(DeviceDataManagerX11Test, NotifyOnDisable) {
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
  TestInputDeviceObserver observer(manager);
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.emplace_back(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         "Keyboard");
  keyboards.emplace_back(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         "Keyboard");
  SetKeyboardDevices(keyboards);
  EXPECT_TRUE(observer.change_notified());
  std::vector<KeyboardDevice> devices = manager->GetKeyboardDevices();
  EXPECT_EQ(keyboards.size(), devices.size());
  observer.Reset();
  // Disable the device, should be notified that the device list contains one
  // less device.
  manager->DisableDevice(static_cast<x11::Input::DeviceId>(2));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(1u, devices.size());
  InputDevice device = devices.front();
  EXPECT_EQ(1, device.id);
  observer.Reset();
  // Reenable the device, should be notified that the device list contains one
  // more device.
  manager->EnableDevice(static_cast<x11::Input::DeviceId>(2));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(keyboards.size(), devices.size());
}

// Tests blocking multiple devices.
TEST_F(DeviceDataManagerX11Test, TestMultipleDisable) {
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
  TestInputDeviceObserver observer(manager);
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.emplace_back(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         "Keyboard");
  keyboards.emplace_back(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         "Keyboard");
  SetKeyboardDevices(keyboards);
  EXPECT_TRUE(observer.change_notified());
  std::vector<KeyboardDevice> devices = manager->GetKeyboardDevices();
  EXPECT_EQ(keyboards.size(), devices.size());
  observer.Reset();
  // Disable the device, should be notified that the device list contains one
  // less device.
  manager->DisableDevice(static_cast<x11::Input::DeviceId>(1));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(1u, devices.size());
  observer.Reset();
  // Disable the second device, should be notified that the device list empty.
  manager->DisableDevice(static_cast<x11::Input::DeviceId>(2));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(0u, devices.size());
  observer.Reset();
  // Enable the first device, should be notified that one device present.
  manager->EnableDevice(static_cast<x11::Input::DeviceId>(1));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(1u, devices.size());
  observer.Reset();
  // Enable the second device, should be notified that both devices present.
  manager->EnableDevice(static_cast<x11::Input::DeviceId>(2));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(2u, devices.size());
}

TEST_F(DeviceDataManagerX11Test, UnblockOnDeviceUnplugged) {
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
  TestInputDeviceObserver observer(manager);
  std::vector<ui::KeyboardDevice> all_keyboards;
  all_keyboards.emplace_back(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                             "Keyboard");
  all_keyboards.emplace_back(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                             "Keyboard");
  SetKeyboardDevices(all_keyboards);
  EXPECT_TRUE(observer.change_notified());
  std::vector<KeyboardDevice> devices = manager->GetKeyboardDevices();
  EXPECT_EQ(all_keyboards.size(), devices.size());
  observer.Reset();
  // Expect to be notified that the device is no longer available.
  manager->DisableDevice(static_cast<x11::Input::DeviceId>(2));
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  EXPECT_EQ(1u, devices.size());
  observer.Reset();
  // Unplug the disabled device. Should not be notified, since the active list
  // did not change.
  std::vector<ui::KeyboardDevice> subset_keyboards;
  subset_keyboards.emplace_back(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                "Keyboard");
  SetKeyboardDevices(subset_keyboards);
  EXPECT_FALSE(observer.change_notified());
  // Replug in the first device. Should be notified of the new device.
  SetKeyboardDevices(all_keyboards);
  EXPECT_TRUE(observer.change_notified());
  devices = manager->GetKeyboardDevices();
  // Both devices now present.
  EXPECT_EQ(2u, devices.size());
}

// Tests that GetEventData handles cases where the event's valuator_mask is
// shorter than the registered valuator's index.
// Regression test for crbug.com/501862016.
TEST_F(DeviceDataManagerX11Test, GetEventDataShortMask) {
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
  const auto device_id = static_cast<x11::Input::DeviceId>(1);

  // Initialize a device with some valuators. SetDeviceListForTest will
  // initialize them with indices starting from 0.
  // Touch major/minor/orientation/pressure/x/y/tracking_id/raw_timestamp
  // are 8 valuators (indices 0-7).
  manager->SetDeviceListForTest({static_cast<int>(device_id)}, {}, {});

  // Manually create an event with an empty mask.
  x11::Event event(false, x11::Input::DeviceEvent{
                              .opcode = x11::Input::DeviceEvent::Motion,
                              .deviceid = device_id,
                              .sourceid = device_id,
                              .valuator_mask = {},
                              .axisvalues = {},
                          });

  double value = -1.0;
  // This should not crash and should return false because any index is OOB
  // for an empty mask.
  EXPECT_FALSE(manager->GetEventData(
      event, DeviceDataManagerX11::DT_TOUCH_MAJOR, &value));
  EXPECT_EQ(value, -1.0);
}

// Regression test for crbug.com/40246366: verifies that initial scroll
// valuator positions and SlaveSwitch updates preserve the baseline position so
// the first scroll tick is not dropped.
TEST_F(DeviceDataManagerX11Test, ScrollClassValuatorBaselinePreserved) {
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();
  const auto device_id = static_cast<x11::Input::DeviceId>(1);
  constexpr uint16_t kHorizValuator = 2;
  constexpr uint16_t kVertValuator = 3;

  SetUpScrollDevice(device_id, kHorizValuator, /*horizontal_increment=*/1,
                    /*horizontal_value=*/0, kVertValuator,
                    /*vertical_increment=*/1, /*vertical_value=*/10);

  // The first scroll tick from initial value 10 to 11 should immediately
  // report a -1.0 vertical offset rather than being swallowed.
  double x_offset = 0.0;
  double y_offset = 0.0;
  manager->GetScrollClassOffsets(
      CreateScrollMotionEvent(device_id, kVertValuator, 11), &x_offset,
      &y_offset);
  EXPECT_DOUBLE_EQ(0.0, x_offset);
  EXPECT_DOUBLE_EQ(-1.0, y_offset);

  // Simulate a SlaveSwitch after the valuator advanced to 25 (e.g. while
  // scrolling outside the window). The first tick after SlaveSwitch to 26
  // should report -1.0 (neither 0.0 nor -15.0).
  manager->UpdateScrollClassValuators(
      device_id, {
                     x11::Input::DeviceClass{
                         .valuator =
                             x11::Input::DeviceClass::Valuator{
                                 .number = kHorizValuator,
                                 .value = {0, 0},
                             },
                     },
                     x11::Input::DeviceClass{
                         .valuator =
                             x11::Input::DeviceClass::Valuator{
                                 .number = kVertValuator,
                                 .value = {25, 0},
                             },
                     },
                 });
  manager->GetScrollClassOffsets(
      CreateScrollMotionEvent(device_id, kVertValuator, 26), &x_offset,
      &y_offset);
  EXPECT_DOUBLE_EQ(0.0, x_offset);
  EXPECT_DOUBLE_EQ(-1.0, y_offset);

  // If valuator classes are unavailable on invalidation, the baseline is
  // marked unseen and re-established on the next event without a jump.
  manager->UpdateScrollClassValuators(device_id, {});
  manager->GetScrollClassOffsets(
      CreateScrollMotionEvent(device_id, kVertValuator, 40), &x_offset,
      &y_offset);
  EXPECT_DOUBLE_EQ(0.0, x_offset);
  EXPECT_DOUBLE_EQ(0.0, y_offset);

  manager->GetScrollClassOffsets(
      CreateScrollMotionEvent(device_id, kVertValuator, 41), &x_offset,
      &y_offset);
  EXPECT_DOUBLE_EQ(0.0, x_offset);
  EXPECT_DOUBLE_EQ(-1.0, y_offset);
}

}  // namespace test
}  // namespace ui
