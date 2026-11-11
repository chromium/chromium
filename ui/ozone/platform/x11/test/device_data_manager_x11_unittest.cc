// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_data_manager_x11.h"

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

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

class NoOpEventDispatcher : public PlatformEventDispatcher {
 public:
  bool CanDispatchEvent(const PlatformEvent& event) override { return false; }
  uint32_t DispatchEvent(const PlatformEvent& event) override {
    return POST_DISPATCH_NONE;
  }
};

class InputDeviceConfigChangedWaiter : public InputDeviceEventObserver {
 public:
  explicit InputDeviceConfigChangedWaiter(DeviceDataManager* manager)
      : manager_(manager) {
    manager_->AddObserver(this);
  }
  ~InputDeviceConfigChangedWaiter() override { manager_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  void OnInputDeviceConfigurationChanged(uint8_t types) override {
    if (types & InputDeviceEventObserver::kKeyboard) {
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<DeviceDataManager> manager_;
  base::RunLoop run_loop_;
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
    DeviceDataManagerX11::GetInstance()->UpdateDeviceList(
        x11::Connection::Get());
  }

  void UpdateDeviceListFromLists(
      x11::Connection* connection,
      const std::vector<x11::Input::DeviceInfo>& dev_list,
      const std::vector<x11::Input::XIDeviceInfo>& info_list) {
    DeviceDataManagerX11::GetInstance()->UpdateDeviceListFromLists(
        connection, dev_list, info_list);
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

// Regression test for crbug.com/489447085: verifies that slave pointer devices
// with absolute X/Y valuators (such as remote-desktop mice, VM tablets, and
// XWayland pointers) are classified as mice rather than pens, while actual
// stylus devices remain classified as pens.
TEST_F(DeviceDataManagerX11Test, AbsolutePointerVsStylusClassification) {
  auto* connection = x11::Connection::Get();
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();

  const x11::Atom abs_x = connection->GetAtom("Abs X");
  const x11::Atom abs_y = connection->GetAtom("Abs Y");
  const x11::Atom abs_pressure = connection->GetAtom("Abs Pressure");
  const x11::Atom abs_tilt_x = connection->GetAtom("Abs Tilt X");
  const x11::Atom abs_tilt_y = connection->GetAtom("Abs Tilt Y");
  const x11::Atom rel_vert_wheel = connection->GetAtom("Rel Vert Wheel");

  auto make_valuator = [](uint16_t number, x11::Atom label, int32_t min_val,
                          int32_t max_val) {
    return x11::Input::DeviceClass{
        .valuator =
            x11::Input::DeviceClass::Valuator{
                .number = number,
                .label = label,
                .min = {min_val, 0},
                .max = {max_val, 0},
                .mode = x11::Input::ValuatorMode::Absolute,
            },
    };
  };

  constexpr auto kMasterId = static_cast<x11::Input::DeviceId>(2);
  constexpr uint8_t kAbsMouseId = 10;
  constexpr uint8_t kXwaylandPointerId = 11;
  constexpr uint8_t kStylusWithPressureId = 12;
  constexpr uint8_t kXwaylandStylusId = 13;
  constexpr uint8_t kBasicStylusId = 14;
  constexpr uint8_t kBasicEraserId = 15;
  constexpr uint8_t kBasicTabletId = 16;
  constexpr uint8_t kEvdevTouchpadWithPressureId = 17;
  constexpr uint8_t kTabletCursorPuckId = 18;
  constexpr uint8_t kTabletPadId = 19;
  constexpr uint8_t kTouchscreenWithPressureId = 20;

  std::vector<x11::Input::DeviceInfo> dev_list = {
      {.device_type = connection->GetAtom("MOUSE"), .device_id = kAbsMouseId},
      {.device_type = connection->GetAtom("xwayland-pointer"),
       .device_id = kXwaylandPointerId},
      {.device_type = connection->GetAtom("STYLUS"),
       .device_id = kStylusWithPressureId},
      // XWayland assigns the first-created device's type atom to subsequent
      // devices (see xwayland-input.c add_device()), so xwayland-tablet stylus
      // can carry the "xwayland-pointer" XI1 type atom.
      {.device_type = connection->GetAtom("xwayland-pointer"),
       .device_id = kXwaylandStylusId},
      {.device_type = connection->GetAtom("STYLUS"),
       .device_id = kBasicStylusId},
      {.device_type = connection->GetAtom("ERASER"),
       .device_id = kBasicEraserId},
      {.device_type = connection->GetAtom("TABLET"),
       .device_id = kBasicTabletId},
      {.device_type = connection->GetAtom("TOUCHPAD"),
       .device_id = kEvdevTouchpadWithPressureId},
      {.device_type = connection->GetAtom("CURSOR"),
       .device_id = kTabletCursorPuckId},
      {.device_type = connection->GetAtom("PAD"), .device_id = kTabletPadId},
      {.device_type = connection->GetAtom("MOUSE"),
       .device_id = kTouchscreenWithPressureId},
  };

  auto id = [](uint8_t raw) { return static_cast<x11::Input::DeviceId>(raw); };
  std::vector<x11::Input::XIDeviceInfo> info_list = {
      {
          .deviceid = kMasterId,
          .type = x11::Input::DeviceType::MasterPointer,
          .enabled = 1,
          .name = "Virtual core pointer",
      },
      {
          .deviceid = id(kAbsMouseId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "PCoIP Absolute Virtual Mouse",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 65535),
                  make_valuator(1, abs_y, 0, 65535),
                  make_valuator(2, rel_vert_wheel, -1, -1),
                  x11::Input::DeviceClass{
                      .scroll =
                          x11::Input::DeviceClass::Scroll{
                              .number = 2,
                              .scroll_type = x11::Input::ScrollType::Vertical,
                              .increment = {1, 0},
                          },
                  },
              },
      },
      {
          .deviceid = id(kXwaylandPointerId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "xwayland-pointer:10",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 65535),
                  make_valuator(1, abs_y, 0, 65535),
              },
      },
      {
          .deviceid = id(kStylusWithPressureId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Wacom Intuos Pen stylus",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
                  make_valuator(2, abs_pressure, 0, 2048),
                  make_valuator(3, abs_tilt_x, -60, 60),
                  make_valuator(4, abs_tilt_y, -60, 60),
              },
      },
      {
          .deviceid = id(kXwaylandStylusId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "xwayland-tablet stylus:10",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 262143),
                  make_valuator(1, abs_y, 0, 262143),
                  make_valuator(2, abs_pressure, 0, 65535),
                  make_valuator(3, abs_tilt_x, -64, 63),
                  make_valuator(4, abs_tilt_y, -64, 63),
              },
      },
      {
          .deviceid = id(kBasicStylusId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Generic Pen stylus",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
              },
      },
      {
          .deviceid = id(kBasicEraserId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Generic Pen eraser",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
              },
      },
      {
          .deviceid = id(kBasicTabletId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Evdev Tablet",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
              },
      },
      {
          .deviceid = id(kEvdevTouchpadWithPressureId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "SynPS/2 Synaptics TouchPad",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 4096),
                  make_valuator(1, abs_y, 0, 4096),
                  make_valuator(2, abs_pressure, 0, 255),
              },
      },
      {
          .deviceid = id(kTabletCursorPuckId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Wacom Intuos cursor",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
                  make_valuator(2, abs_pressure, 0, 2048),
              },
      },
      {
          .deviceid = id(kTabletPadId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Wacom Intuos pad",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 32767),
                  make_valuator(1, abs_y, 0, 32767),
                  make_valuator(2, abs_pressure, 0, 2048),
              },
      },
      {
          .deviceid = id(kTouchscreenWithPressureId),
          .type = x11::Input::DeviceType::SlavePointer,
          .attachment = kMasterId,
          .enabled = 1,
          .name = "Direct Touchscreen",
          .classes =
              {
                  make_valuator(0, abs_x, 0, 4096),
                  make_valuator(1, abs_y, 0, 4096),
                  make_valuator(2, abs_pressure, 0, 255),
                  x11::Input::DeviceClass{
                      .touch =
                          x11::Input::DeviceClass::Touch{
                              .mode = x11::Input::TouchMode::Direct,
                              .num_touches = 10,
                          },
                  },
              },
      },
  };

  UpdateDeviceListFromLists(connection, dev_list, info_list);

  auto make_motion =
      [&](uint8_t source_id, uint32_t mask = 0x3,
          std::vector<x11::Input::Fp3232> values = {{100, 0}, {200, 0}}) {
        return x11::Event(false, x11::Input::DeviceEvent{
                                     .opcode = x11::Input::DeviceEvent::Motion,
                                     .deviceid = kMasterId,
                                     .sourceid = id(source_id),
                                     .valuator_mask = {mask},
                                     .axisvalues = std::move(values),
                                 });
      };

  // Absolute mice and XWayland pointers must be classified as kMouse.
  x11::Event abs_mouse_event = make_motion(kAbsMouseId);
  EXPECT_FALSE(manager->IsStylusXInputEvent(abs_mouse_event));
  EXPECT_EQ(EventPointerType::kMouse,
            GetStylusPointerDetailsFromXEvent(abs_mouse_event).pointer_type);

  x11::Event xwayland_pointer_event = make_motion(kXwaylandPointerId);
  EXPECT_FALSE(manager->IsStylusXInputEvent(xwayland_pointer_event));
  EXPECT_EQ(
      EventPointerType::kMouse,
      GetStylusPointerDetailsFromXEvent(xwayland_pointer_event).pointer_type);

  // Stylus with pressure and tilt valuators reports kPen and normalized values.
  x11::Event stylus_event =
      make_motion(kStylusWithPressureId, /*mask=*/0x1f,
                  {{100, 0}, {200, 0}, {1024, 0}, {15, 0}, {-20, 0}});
  EXPECT_TRUE(manager->IsStylusXInputEvent(stylus_event));
  PointerDetails stylus_details =
      GetStylusPointerDetailsFromXEvent(stylus_event);
  EXPECT_EQ(EventPointerType::kPen, stylus_details.pointer_type);
  EXPECT_FLOAT_EQ(0.5f, stylus_details.force);
  EXPECT_FLOAT_EQ(15.0f, stylus_details.tilt_x);
  EXPECT_FLOAT_EQ(-20.0f, stylus_details.tilt_y);

  // XWayland tablet stylus is classified as kPen via pressure/tilt valuators.
  x11::Event xwayland_stylus_event = make_motion(kXwaylandStylusId);
  EXPECT_TRUE(manager->IsStylusXInputEvent(xwayland_stylus_event));
  EXPECT_EQ(
      EventPointerType::kPen,
      GetStylusPointerDetailsFromXEvent(xwayland_stylus_event).pointer_type);

  // Stylus/eraser/tablet devices without pressure or tilt axes remain kPen.
  for (uint8_t stylus_id : {kBasicStylusId, kBasicEraserId, kBasicTabletId}) {
    x11::Event basic_stylus_event = make_motion(stylus_id);
    EXPECT_TRUE(manager->IsStylusXInputEvent(basic_stylus_event))
        << "device_id=" << static_cast<int>(stylus_id);
    EXPECT_EQ(
        EventPointerType::kPen,
        GetStylusPointerDetailsFromXEvent(basic_stylus_event).pointer_type);
  }

  // Touchpads, tablet pucks/pads, and touchscreens exposing "Abs Pressure" must
  // not be misclassified as styluses.
  for (uint8_t non_stylus_id :
       {kEvdevTouchpadWithPressureId, kTabletCursorPuckId, kTabletPadId,
        kTouchscreenWithPressureId}) {
    x11::Event non_stylus_event = make_motion(non_stylus_id);
    EXPECT_FALSE(manager->IsStylusXInputEvent(non_stylus_event))
        << "device_id=" << static_cast<int>(non_stylus_id);
    EXPECT_EQ(EventPointerType::kMouse,
              GetStylusPointerDetailsFromXEvent(non_stylus_event).pointer_type);
  }
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

TEST_F(DeviceDataManagerX11Test,
       HierarchyEventDoesNotBlockWhenServerUnresponsive) {
  base::test::TaskEnvironment task_env(
      base::test::TaskEnvironment::MainThreadType::UI);
  auto* connection = x11::Connection::Get();
  DeviceDataManagerX11* manager = DeviceDataManagerX11::GetInstance();

  SetKeyboardDevices({KeyboardDevice(99, INPUT_DEVICE_INTERNAL, "Fake")});
  X11EventSource event_source(connection);
  NoOpEventDispatcher dispatcher;
  {
    InputDeviceConfigChangedWaiter initial_waiter(manager);
    event_source.AddPlatformEventDispatcher(&dispatcher);
    initial_waiter.Wait();
  }
  EXPECT_TRUE(manager->GetKeyboardDevices().empty());

  const auto fake_cmt_id = static_cast<x11::Input::DeviceId>(99);
  manager->SetDeviceListForTest({}, {static_cast<int>(fake_cmt_id)}, {});
  SetKeyboardDevices({KeyboardDevice(99, INPUT_DEVICE_INTERNAL, "Fake")});
  x11::Event cmt_check_event(false,
                             x11::Input::DeviceEvent{
                                 .opcode = x11::Input::DeviceEvent::Motion,
                                 .deviceid = fake_cmt_id,
                                 .sourceid = fake_cmt_id,
                             });
  ASSERT_TRUE(manager->IsCMTDeviceEvent(cmt_check_event));

  // Grab the X server on a separate connection so `connection` receives no
  // replies until UngrabServer(). A synchronous WaitForResponse() on
  // `connection` will block indefinitely here.
  auto grabber = connection->Clone();
  grabber->GrabServer();
  grabber->Sync();

  InputDeviceConfigChangedWaiter hotplug_waiter(manager);
  x11::Event hierarchy_event(false, x11::Input::HierarchyEvent{});
  x11::Event device_changed_event(
      false, x11::Input::DeviceChangedEvent{
                 .reason = x11::Input::ChangeReason::DeviceChange,
             });
  connection->DispatchEvent(hierarchy_event);
  connection->DispatchEvent(device_changed_event);
  connection->Flush();

  // Cached device data stays intact while the refresh replies are pending.
  EXPECT_TRUE(manager->IsCMTDeviceEvent(cmt_check_event));
  EXPECT_EQ(1u, manager->GetKeyboardDevices().size());

  grabber->UngrabServer();
  grabber->Sync();
  connection->Sync();
  connection->DispatchAll();
  hotplug_waiter.Wait();

  EXPECT_FALSE(manager->IsCMTDeviceEvent(cmt_check_event));
  EXPECT_TRUE(manager->GetKeyboardDevices().empty());
  EXPECT_FALSE(
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(connection).empty());

  event_source.RemovePlatformEventDispatcher(&dispatcher);
}

TEST_F(DeviceDataManagerX11Test, UpdateDeviceListBurstCoalescesCallbacks) {
  auto* connection = x11::Connection::Get();
  auto* cache = DeviceListCacheX11::GetInstance();

  auto grabber = connection->Clone();
  grabber->GrabServer();
  grabber->Sync();

  int first_callbacks = 0;
  int second_callbacks = 0;
  cache->UpdateDeviceList(
      connection,
      base::BindOnce([](int* count) { ++(*count); }, &first_callbacks));
  cache->UpdateDeviceList(
      connection,
      base::BindOnce([](int* count) { ++(*count); }, &second_callbacks));

  EXPECT_EQ(0, first_callbacks);
  EXPECT_EQ(0, second_callbacks);

  grabber->UngrabServer();
  grabber->Sync();
  connection->Sync();
  connection->DispatchAll();

  EXPECT_EQ(1, first_callbacks);
  EXPECT_EQ(1, second_callbacks);
  EXPECT_FALSE(cache->GetXDeviceList(connection).empty());
  EXPECT_FALSE(cache->GetXI2DeviceList(connection).empty());
}

}  // namespace test
}  // namespace ui
