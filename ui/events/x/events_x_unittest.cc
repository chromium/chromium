// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <utility>

#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

// Initializes the passed-in Xlib event.
void InitButtonEvent(XEvent* event,
                     bool is_press,
                     const gfx::Point& location,
                     int button,
                     int state) {
  memset(event, 0, sizeof(*event));

  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  XButtonEvent* button_event = &(event->xbutton);
  button_event->type = is_press ? ButtonPress : ButtonRelease;
  button_event->x = location.x();
  button_event->y = location.y();
  button_event->button = button;
  button_event->state = state;
}

#if !defined(OS_CHROMEOS)
// Initializes the passed-in Xlib event.
void InitKeyEvent(Display* display,
                  XEvent* event,
                  bool is_press,
                  int keycode,
                  int state) {
  memset(event, 0, sizeof(*event));

  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  XKeyEvent* key_event = &(event->xkey);
  key_event->display = display;
  key_event->type = is_press ? KeyPress : KeyRelease;
  key_event->keycode = keycode;
  key_event->state = state;
}
#endif

float ComputeRotationAngle(float twist) {
  float rotation_angle = twist;
  while (rotation_angle < 0)
    rotation_angle += 180.f;
  while (rotation_angle >= 180)
    rotation_angle -= 180.f;
  return rotation_angle;
}

}  // namespace

class EventsXTest : public testing::Test {
 public:
  EventsXTest() {}
  ~EventsXTest() override {}

  void SetUp() override {
    DeviceDataManagerX11::CreateInstance();
    ui::TouchFactory::GetInstance()->ResetForTest();
    ResetTimestampRolloverCountersForTesting();
  }
  void TearDown() override { ResetTimestampRolloverCountersForTesting(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(EventsXTest);
};

TEST_F(EventsXTest, ButtonEvents) {
  XEvent event;
  gfx::Point location(5, 10);
  gfx::Vector2d offset;

  InitButtonEvent(&event, true, location, 1, 0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));

  InitButtonEvent(&event, true, location, 2, Button1Mask | ShiftMask);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
                ui::EF_SHIFT_DOWN,
            ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));

  InitButtonEvent(&event, false, location, 3, 0);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));

  // Scroll up.
  InitButtonEvent(&event, true, location, 4, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  InitButtonEvent(&event, true, location, 5, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left.
  InitButtonEvent(&event, true, location, 6, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);

  // Scroll right.
  InitButtonEvent(&event, true, location, 7, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(&event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromNative(&event));
  EXPECT_EQ(location, gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)));
  offset = ui::GetMouseWheelOffset(&event);
  EXPECT_EQ(0, offset.y());
  EXPECT_LT(offset.x(), 0);

  // TODO(derat): Test XInput code.
}

TEST_F(EventsXTest, AvoidExtraEventsOnWheelRelease) {
  XEvent event;
  gfx::Point location(5, 10);

  InitButtonEvent(&event, true, location, 4, 0);
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromNative(&event));

  // We should return ET_UNKNOWN for the release event instead of returning
  // ET_MOUSEWHEEL; otherwise we'll scroll twice for each scrollwheel step.
  InitButtonEvent(&event, false, location, 4, 0);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromNative(&event));

  // TODO(derat): Test XInput code.
}

TEST_F(EventsXTest, EnterLeaveEvent) {
  XEvent event;
  event.xcrossing.type = EnterNotify;
  event.xcrossing.x = 10;
  event.xcrossing.y = 20;
  event.xcrossing.x_root = 110;
  event.xcrossing.y_root = 120;

  // Mouse enter events are converted to mouse move events to be consistent with
  // the way views handle mouse enter. See comments for EnterNotify case in
  // ui::EventTypeFromNative for more details.
  EXPECT_EQ(ui::ET_MOUSE_MOVED, ui::EventTypeFromNative(&event));
  EXPECT_TRUE(ui::EventFlagsFromNative(&event) & ui::EF_IS_SYNTHESIZED);
  EXPECT_EQ(
      "10,20",
      gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)).ToString());
  EXPECT_EQ("110,120", ui::EventSystemLocationFromNative(&event).ToString());

  event.xcrossing.type = LeaveNotify;
  event.xcrossing.x = 30;
  event.xcrossing.y = 40;
  event.xcrossing.x_root = 230;
  event.xcrossing.y_root = 240;
  EXPECT_EQ(ui::ET_MOUSE_EXITED, ui::EventTypeFromNative(&event));
  EXPECT_EQ(
      "30,40",
      gfx::ToFlooredPoint(ui::EventLocationFromNative(&event)).ToString());
  EXPECT_EQ("230,240", ui::EventSystemLocationFromNative(&event).ToString());
}

TEST_F(EventsXTest, ClickCount) {
  XEvent event;
  gfx::Point location(5, 10);

  base::TimeDelta time_stamp = base::TimeTicks::Now().since_origin() -
                               base::TimeDelta::FromMilliseconds(10);
  for (int i = 1; i <= 3; ++i) {
    InitButtonEvent(&event, true, location, 1, 0);
    {
      event.xbutton.time = time_stamp.InMilliseconds() & UINT32_MAX;
      MouseEvent mouseev(&event);
      EXPECT_EQ(ui::ET_MOUSE_PRESSED, mouseev.type());
      EXPECT_EQ(i, mouseev.GetClickCount());
    }

    InitButtonEvent(&event, false, location, 1, 0);
    {
      event.xbutton.time = time_stamp.InMilliseconds();
      MouseEvent mouseev(&event);
      EXPECT_EQ(ui::ET_MOUSE_RELEASED, mouseev.type());
      EXPECT_EQ(i, mouseev.GetClickCount());
    }
    time_stamp += base::TimeDelta::FromMilliseconds(1);
  }
}

TEST_F(EventsXTest, TouchEventBasic) {
  std::vector<int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<Valuator> valuators;

  // Init touch begin with tracking id 5, touch id 0.
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_MAJOR, 20));
  valuators.push_back(
      Valuator(DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.3f));
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 100));
  ui::ScopedXI2Event scoped_xevent;
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 5, gfx::Point(10, 10), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("10,10",
            gfx::ToFlooredPoint(ui::EventLocationFromNative(scoped_xevent))
                .ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  PointerDetails pointer_details =
      GetTouchPointerDetailsFromNative(scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.15f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.1f);

  // Touch update, with new orientation info.
  valuators.clear();
  valuators.push_back(
      Valuator(DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.5f));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchUpdate, 5, gfx::Point(20, 20), valuators);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("20,20",
            gfx::ToFlooredPoint(ui::EventLocationFromNative(scoped_xevent))
                .ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  pointer_details = GetTouchPointerDetailsFromNative(scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.25f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.1f);

  // Another touch with tracking id 6, touch id 1.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_MAJOR, 100));
  valuators.push_back(Valuator(
      DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.9f));
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 500));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("200,200",
            gfx::ToFlooredPoint(ui::EventLocationFromNative(scoped_xevent))
                .ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 1);
  pointer_details = GetTouchPointerDetailsFromNative(scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.45f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 50.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.5f);

  // Touch with tracking id 5 should have old radius/angle value and new pressue
  // value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 50));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 5, gfx::Point(30, 30), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("30,30",
            gfx::ToFlooredPoint(ui::EventLocationFromNative(scoped_xevent))
                .ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 0);
  pointer_details = GetTouchPointerDetailsFromNative(scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.25f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.f);

  // Touch with tracking id 6 should have old angle/pressure value and new
  // radius value.
  valuators.clear();
  valuators.push_back(Valuator(DeviceDataManagerX11::DT_TOUCH_MAJOR, 50));
  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 6, gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromNative(scoped_xevent));
  EXPECT_EQ("200,200",
            gfx::ToFlooredPoint(ui::EventLocationFromNative(scoped_xevent))
                .ToString());
  EXPECT_EQ(GetTouchId(scoped_xevent), 1);
  pointer_details = GetTouchPointerDetailsFromNative(scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.45f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 25.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.f);
}

int GetTouchIdForTrackingId(uint32_t tracking_id) {
  int slot = 0;
  bool success =
      TouchFactory::GetInstance()->QuerySlotForTrackingID(tracking_id, &slot);
  if (success)
    return slot;
  return -1;
}

TEST_F(EventsXTest, TouchEventNotRemovingFromNativeMapping) {
  const int kTrackingId = 5;
  const int kDeviceId = 0;

  std::vector<int> devices{kDeviceId};
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<Valuator> valuators;

  // Two touch presses with the same tracking id.
  ui::ScopedXI2Event xpress0;
  xpress0.InitTouchEvent(kDeviceId, XI_TouchBegin, kTrackingId,
                         gfx::Point(10, 10), valuators);
  std::unique_ptr<ui::TouchEvent> upress0(new ui::TouchEvent(xpress0));
  EXPECT_EQ(kDeviceId, GetTouchIdForTrackingId(kTrackingId));

  ui::ScopedXI2Event xpress1;
  xpress1.InitTouchEvent(kDeviceId, XI_TouchBegin, kTrackingId,
                         gfx::Point(20, 20), valuators);
  ui::TouchEvent upress1(xpress1);
  EXPECT_EQ(kDeviceId, GetTouchIdForTrackingId(kTrackingId));

  // The second touch release should clear the mapping from the
  // tracking id.
  ui::ScopedXI2Event xrelease1;
  xrelease1.InitTouchEvent(kDeviceId, XI_TouchEnd, kTrackingId,
                           gfx::Point(10, 10), valuators);
  {
    ui::TouchEvent urelease1(xrelease1);
  }
  EXPECT_EQ(-1, GetTouchIdForTrackingId(kTrackingId));
}

// Copied events should not remove native touch id mappings, as this causes a
// crash (crbug.com/467102). Copied events do not contain a proper
// PlatformEvent and should not attempt to access it.
TEST_F(EventsXTest, CopiedTouchEventNotRemovingFromNativeMapping) {
  std::vector<int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<Valuator> valuators;

  // Create a release event which has a native touch id mapping.
  ui::ScopedXI2Event xrelease0;
  xrelease0.InitTouchEvent(0, XI_TouchEnd, 0, gfx::Point(10, 10), valuators);
  ui::TouchEvent urelease0(xrelease0);
  {
    // When the copy is destructed it should not attempt to remove the mapping.
    // Exiting this scope should not cause a crash.
    ui::TouchEvent copy = urelease0;
  }
}

// Verifies that the type of events from a disabled keyboard is ET_UNKNOWN, but
// that an exception list of keys can still be processed.
TEST_F(EventsXTest, DisableKeyboard) {
  DeviceDataManagerX11* device_data_manager =
      static_cast<DeviceDataManagerX11*>(
          DeviceDataManager::GetInstance());
  int blocked_device_id = 1;
  int other_device_id = 2;
  int master_device_id = 3;
  device_data_manager->DisableDevice(blocked_device_id);

  std::unique_ptr<std::set<KeyboardCode>> excepted_keys(
      new std::set<KeyboardCode>);
  excepted_keys->insert(VKEY_B);
  device_data_manager->SetDisabledKeyboardAllowedKeys(std::move(excepted_keys));

  ScopedXI2Event xev;
  // A is not allowed on the blocked keyboard, and should return ET_UNKNOWN.
  xev.InitGenericKeyEvent(master_device_id,
                          blocked_device_id,
                          ui::ET_KEY_PRESSED,
                          ui::VKEY_A,
                          0);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromNative(xev));

  // The B key is allowed as an exception, and should return KEY_PRESSED.
  xev.InitGenericKeyEvent(master_device_id,
                          blocked_device_id,
                          ui::ET_KEY_PRESSED,
                          ui::VKEY_B,
                          0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(xev));

  // Both A and B are allowed on an unblocked keyboard device.
  xev.InitGenericKeyEvent(master_device_id,
                          other_device_id,
                          ui::ET_KEY_PRESSED,
                          ui::VKEY_A,
                          0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(xev));
  xev.InitGenericKeyEvent(master_device_id,
                          other_device_id,
                          ui::ET_KEY_PRESSED,
                          ui::VKEY_B,
                          0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(xev));

  device_data_manager->EnableDevice(blocked_device_id);
  device_data_manager->SetDisabledKeyboardAllowedKeys(nullptr);

  // A key returns KEY_PRESSED as per usual now that keyboard was re-enabled.
  xev.InitGenericKeyEvent(master_device_id,
                          blocked_device_id,
                          ui::ET_KEY_PRESSED,
                          ui::VKEY_A,
                          0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(xev));
}

// Verifies that the type of events from a disabled mouse is ET_UNKNOWN.
TEST_F(EventsXTest, DisableMouse) {
  DeviceDataManagerX11* device_data_manager =
      static_cast<DeviceDataManagerX11*>(
          DeviceDataManager::GetInstance());
  int blocked_device_id = 1;
  int other_device_id = 2;
  std::vector<int> device_list;
  device_list.push_back(blocked_device_id);
  device_list.push_back(other_device_id);
  TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);

  device_data_manager->DisableDevice(blocked_device_id);

  ScopedXI2Event xev;
  xev.InitGenericButtonEvent(blocked_device_id, ET_MOUSE_PRESSED, gfx::Point(),
      EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromNative(xev));

  xev.InitGenericButtonEvent(other_device_id, ET_MOUSE_PRESSED, gfx::Point(),
      EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(xev));

  device_data_manager->EnableDevice(blocked_device_id);

  xev.InitGenericButtonEvent(blocked_device_id, ET_MOUSE_PRESSED, gfx::Point(),
      EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(xev));
}

#if !defined(OS_CHROMEOS)
TEST_F(EventsXTest, ImeFabricatedKeyEvents) {
  Display* display = gfx::GetXDisplay();

  unsigned int state_to_be_fabricated[] = {
    0, ShiftMask, LockMask, ShiftMask | LockMask,
  };
  for (size_t i = 0; i < base::size(state_to_be_fabricated); ++i) {
    unsigned int state = state_to_be_fabricated[i];
    for (int is_char = 0; is_char < 2; ++is_char) {
      XEvent x_event;
      InitKeyEvent(display, &x_event, true, 0, state);
      ui::KeyEvent key_event(&x_event);
      if (is_char) {
        KeyEventTestApi test_event(&key_event);
        test_event.set_is_char(true);
      }
      EXPECT_TRUE(key_event.flags() & ui::EF_IME_FABRICATED_KEY);
    }
  }

  unsigned int state_to_be_not_fabricated[] = {
    ControlMask, Mod1Mask, Mod2Mask, ShiftMask | ControlMask,
  };
  for (size_t i = 0; i < base::size(state_to_be_not_fabricated); ++i) {
    unsigned int state = state_to_be_not_fabricated[i];
    for (int is_char = 0; is_char < 2; ++is_char) {
      XEvent x_event;
      InitKeyEvent(display, &x_event, true, 0, state);
      ui::KeyEvent key_event(&x_event);
      if (is_char) {
        KeyEventTestApi test_event(&key_event);
        test_event.set_is_char(true);
      }
      EXPECT_FALSE(key_event.flags() & ui::EF_IME_FABRICATED_KEY);
    }
  }
}
#endif

TEST_F(EventsXTest, IgnoresMotionEventForMouseWheelScroll) {
  int device_id = 1;
  std::vector<int> devices;
  devices.push_back(device_id);
  ui::SetUpPointerDevicesForTest(devices);

  ScopedXI2Event xev;
  xev.InitScrollEvent(device_id, 1, 2, 3, 4, 1);
  // We shouldn't produce a mouse move event on a mouse wheel
  // scroll. These events are only produced for some mice.
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromNative(xev));
}

namespace {

// Returns a fake TimeTicks based on the given millisecond offset.
base::TimeTicks TimeTicksFromMillis(int64_t millis) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(millis);
}

}  // namespace

TEST_F(EventsXTest, TimestampRolloverAndAdjustWhenDecreasing) {
  XEvent event;
  InitButtonEvent(&event, true, gfx::Point(5, 10), 1, 0);

  test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(TimeTicksFromMillis(0x100000001));
  ResetTimestampRolloverCountersForTesting();

  event.xbutton.time = 0xFFFFFFFF;
  EXPECT_EQ(TimeTicksFromMillis(0xFFFFFFFF), ui::EventTimeFromNative(&event));

  clock.SetNowTicks(TimeTicksFromMillis(0x100000007));
  ResetTimestampRolloverCountersForTesting();

  event.xbutton.time = 3;
  EXPECT_EQ(TimeTicksFromMillis(0x100000000 + 3),
            ui::EventTimeFromNative(&event));
}

TEST_F(EventsXTest, NoTimestampRolloverWhenMonotonicIncreasing) {
  XEvent event;
  InitButtonEvent(&event, true, gfx::Point(5, 10), 1, 0);

  test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(TimeTicksFromMillis(10));
  ResetTimestampRolloverCountersForTesting();

  event.xbutton.time = 6;
  EXPECT_EQ(TimeTicksFromMillis(6), ui::EventTimeFromNative(&event));
  event.xbutton.time = 7;
  EXPECT_EQ(TimeTicksFromMillis(7), ui::EventTimeFromNative(&event));

  clock.SetNowTicks(TimeTicksFromMillis(0x100000005));
  ResetTimestampRolloverCountersForTesting();

  event.xbutton.time = 0xFFFFFFFF;
  EXPECT_EQ(TimeTicksFromMillis(0xFFFFFFFF), ui::EventTimeFromNative(&event));
}

}  // namespace ui
