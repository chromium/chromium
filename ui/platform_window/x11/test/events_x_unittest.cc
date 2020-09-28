// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <xcb/xcb.h>

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
#include "ui/events/types/event_type.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

// Initializes the passed-in Xlib event.
void InitButtonEvent(x11::Event* event,
                     bool is_press,
                     const gfx::Point& location,
                     int button,
                     x11::KeyButMask state) {
  xcb_generic_event_t generic_event;
  memset(&generic_event, 0, sizeof(generic_event));
  auto* button_event =
      reinterpret_cast<xcb_button_press_event_t*>(&generic_event);

  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  button_event->response_type =
      is_press ? x11::ButtonEvent::Press : x11::ButtonEvent::Release;
  button_event->event_x = location.x();
  button_event->event_y = location.y();
  button_event->detail = button;
  button_event->state = static_cast<uint16_t>(state);

  *event = x11::Event(&generic_event, x11::Connection::Get());
}

#if !defined(OS_CHROMEOS)
// Initializes the passed-in x11::Event.
void InitKeyEvent(Display* display,
                  x11::Event* event,
                  bool is_press,
                  int keycode,
                  x11::KeyButMask state) {
  xcb_generic_event_t generic_event;
  memset(&generic_event, 0, sizeof(generic_event));
  auto* key_event = reinterpret_cast<xcb_key_press_event_t*>(&generic_event);

  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  key_event->response_type =
      is_press ? x11::KeyEvent::Press : x11::KeyEvent::Release;
  key_event->detail = keycode;
  key_event->state = static_cast<uint16_t>(state);

  *event = x11::Event(&generic_event, x11::Connection::Get());
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

std::string FlooredEventLocationString(const x11::Event& xev) {
  return gfx::ToFlooredPoint(gfx::PointF(ui::EventLocationFromXEvent(xev)))
      .ToString();
}

}  // namespace

class EventsXTest : public testing::Test {
 public:
  EventsXTest() = default;
  ~EventsXTest() override = default;

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
  x11::Event event;
  gfx::Point location(5, 10);
  gfx::Vector2d offset;

  InitButtonEvent(&event, true, location, 1, {});
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  InitButtonEvent(&event, true, location, 2,
                  x11::KeyButMask::Button1 | x11::KeyButMask::Shift);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
      ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  InitButtonEvent(&event, false, location, 3, {});
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  // Scroll up.
  InitButtonEvent(&event, true, location, 4, {});
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  InitButtonEvent(&event, true, location, 5, {});
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left.
  InitButtonEvent(&event, true, location, 6, {});
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);

  // Scroll right.
  InitButtonEvent(&event, true, location, 7, {});
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_LT(offset.x(), 0);

  // TODO(derat): Test XInput code.
}

TEST_F(EventsXTest, AvoidExtraEventsOnWheelRelease) {
  x11::Event event;
  gfx::Point location(5, 10);

  InitButtonEvent(&event, true, location, 4, {});
  EXPECT_EQ(ui::ET_MOUSEWHEEL, ui::EventTypeFromXEvent(event));

  // We should return ET_UNKNOWN for the release event instead of returning
  // ET_MOUSEWHEEL; otherwise we'll scroll twice for each scrollwheel step.
  InitButtonEvent(&event, false, location, 4, {});
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromXEvent(event));

  // TODO(derat): Test XInput code.
}

TEST_F(EventsXTest, EnterLeaveEvent) {
  auto* connection = x11::Connection::Get();
  xcb_generic_event_t ge;
  memset(&ge, 0, sizeof(ge));
  auto* enter = reinterpret_cast<xcb_enter_notify_event_t*>(&ge);
  enter->response_type = x11::CrossingEvent::EnterNotify;
  enter->event_x = 10;
  enter->event_y = 20;
  enter->root_x = 110;
  enter->root_y = 120;
  x11::Event event(&ge, connection);

  // Mouse enter events are converted to mouse move events to be consistent with
  // the way views handle mouse enter. See comments for EnterNotify case in
  // ui::EventTypeFromXEvent for more details.
  EXPECT_EQ(ui::ET_MOUSE_MOVED, ui::EventTypeFromXEvent(event));
  EXPECT_TRUE(ui::EventFlagsFromXEvent(event) & ui::EF_IS_SYNTHESIZED);
  EXPECT_EQ("10,20", ui::EventLocationFromXEvent(event).ToString());
  EXPECT_EQ("110,120", ui::EventSystemLocationFromXEvent(event).ToString());

  enter->response_type = x11::CrossingEvent::LeaveNotify;
  enter->event_x = 30;
  enter->event_y = 40;
  enter->root_x = 230;
  enter->root_y = 240;
  event = x11::Event(&ge, connection);
  EXPECT_EQ(ui::ET_MOUSE_EXITED, ui::EventTypeFromXEvent(event));
  EXPECT_EQ("30,40", ui::EventLocationFromXEvent(event).ToString());
  EXPECT_EQ("230,240", ui::EventSystemLocationFromXEvent(event).ToString());
}

TEST_F(EventsXTest, ClickCount) {
  x11::Event event;
  gfx::Point location(5, 10);

  base::TimeDelta time_stamp = base::TimeTicks::Now().since_origin() -
                               base::TimeDelta::FromMilliseconds(10);
  for (int i = 1; i <= 3; ++i) {
    InitButtonEvent(&event, true, location, 1, {});
    {
      uint32_t time = time_stamp.InMilliseconds() & UINT32_MAX;
      event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(time);
      auto mouseev = ui::BuildMouseEventFromXEvent(event);
      EXPECT_EQ(ui::ET_MOUSE_PRESSED, mouseev->type());
      EXPECT_EQ(i, mouseev->GetClickCount());
    }

    InitButtonEvent(&event, false, location, 1, {});
    {
      uint32_t time = time_stamp.InMilliseconds() & UINT32_MAX;
      event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(time);
      auto mouseev = ui::BuildMouseEventFromXEvent(event);
      EXPECT_EQ(ui::ET_MOUSE_RELEASED, mouseev->type());
      EXPECT_EQ(i, mouseev->GetClickCount());
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
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_MAJOR, 20);
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.3f);
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 100);
  ui::ScopedXI2Event scoped_xevent;
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchBegin, 5,
                               gfx::Point(10, 10), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromXEvent(*scoped_xevent));
  EXPECT_EQ("10,10", FlooredEventLocationString(*scoped_xevent));
  EXPECT_EQ(GetTouchIdFromXEvent(*scoped_xevent), 0);
  PointerDetails pointer_details =
      GetTouchPointerDetailsFromXEvent(*scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.15f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.1f);

  // Touch update, with new orientation info.
  valuators.clear();
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.5f);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchUpdate, 5,
                               gfx::Point(20, 20), valuators);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ui::EventTypeFromXEvent(*scoped_xevent));
  EXPECT_EQ("20,20", FlooredEventLocationString(*scoped_xevent));
  EXPECT_EQ(GetTouchIdFromXEvent(*scoped_xevent), 0);
  pointer_details = GetTouchPointerDetailsFromXEvent(*scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.25f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.1f);

  // Another touch with tracking id 6, touch id 1.
  valuators.clear();
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_MAJOR, 100);
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.9f);
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 500);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchBegin, 6,
                               gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ui::EventTypeFromXEvent(*scoped_xevent));
  EXPECT_EQ("200,200", FlooredEventLocationString(*scoped_xevent));
  EXPECT_EQ(GetTouchIdFromXEvent(*scoped_xevent), 1);
  pointer_details = GetTouchPointerDetailsFromXEvent(*scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.45f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 50.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.5f);

  // Touch with tracking id 5 should have old radius/angle value and new pressue
  // value.
  valuators.clear();
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_PRESSURE, 50);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchEnd, 5,
                               gfx::Point(30, 30), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromXEvent(*scoped_xevent));
  EXPECT_EQ("30,30", FlooredEventLocationString(*scoped_xevent));
  EXPECT_EQ(GetTouchIdFromXEvent(*scoped_xevent), 0);
  pointer_details = GetTouchPointerDetailsFromXEvent(*scoped_xevent);
  EXPECT_FLOAT_EQ(ComputeRotationAngle(pointer_details.twist), 0.25f);
  EXPECT_FLOAT_EQ(pointer_details.radius_x, 10.0f);
  EXPECT_FLOAT_EQ(pointer_details.force, 0.f);

  // Touch with tracking id 6 should have old angle/pressure value and new
  // radius value.
  valuators.clear();
  valuators.emplace_back(DeviceDataManagerX11::DT_TOUCH_MAJOR, 50);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchEnd, 6,
                               gfx::Point(200, 200), valuators);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ui::EventTypeFromXEvent(*scoped_xevent));
  EXPECT_EQ("200,200", FlooredEventLocationString(*scoped_xevent));
  EXPECT_EQ(GetTouchIdFromXEvent(*scoped_xevent), 1);
  pointer_details = GetTouchPointerDetailsFromXEvent(*scoped_xevent);
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
  xpress0.InitTouchEvent(kDeviceId, x11::Input::DeviceEvent::TouchBegin,
                         kTrackingId, gfx::Point(10, 10), valuators);
  auto upress0 = ui::BuildTouchEventFromXEvent(*xpress0);
  EXPECT_EQ(kDeviceId, GetTouchIdForTrackingId(kTrackingId));

  ui::ScopedXI2Event xpress1;
  xpress1.InitTouchEvent(kDeviceId, x11::Input::DeviceEvent::TouchBegin,
                         kTrackingId, gfx::Point(20, 20), valuators);
  auto upress1 = ui::BuildTouchEventFromXEvent(*xpress1);
  EXPECT_EQ(kDeviceId, GetTouchIdForTrackingId(kTrackingId));

  // The second touch release should clear the mapping from the
  // tracking id.
  ui::ScopedXI2Event xrelease1;
  xrelease1.InitTouchEvent(kDeviceId, x11::Input::DeviceEvent::TouchEnd,
                           kTrackingId, gfx::Point(10, 10), valuators);
  { auto urelease1 = ui::BuildTouchEventFromXEvent(*xrelease1); }
  EXPECT_EQ(-1, GetTouchIdForTrackingId(kTrackingId));
}

// Copied events should not remove native touch id mappings, as this causes a
// crash (crbug.com/467102). Copied events do not contain a proper
// PlatformEvent and should not attempt to access it.
TEST_F(EventsXTest, CopiedTouchEventNotRemovingFromXEventMapping) {
  std::vector<int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<Valuator> valuators;

  // Create a release event which has a native touch id mapping.
  ui::ScopedXI2Event xrelease0;
  xrelease0.InitTouchEvent(0, x11::Input::DeviceEvent::TouchEnd, 0,
                           gfx::Point(10, 10), valuators);
  auto urelease0 = ui::BuildTouchEventFromXEvent(*xrelease0);
  {
    // When the copy is destructed it should not attempt to remove the mapping.
    // Exiting this scope should not cause a crash.
    TouchEvent copy = *urelease0;
  }
}

// Verifies that the type of events from a disabled keyboard is ET_UNKNOWN, but
// that an exception list of keys can still be processed.
TEST_F(EventsXTest, DisableKeyboard) {
  DeviceDataManagerX11* device_data_manager =
      static_cast<DeviceDataManagerX11*>(DeviceDataManager::GetInstance());
  int blocked_device_id = 1;
  auto blocked_device = static_cast<x11::Input::DeviceId>(blocked_device_id);
  int other_device_id = 2;
  int master_device_id = 3;
  device_data_manager->DisableDevice(blocked_device);

  std::unique_ptr<std::set<KeyboardCode>> excepted_keys(
      new std::set<KeyboardCode>);
  excepted_keys->insert(VKEY_B);
  device_data_manager->SetDisabledKeyboardAllowedKeys(std::move(excepted_keys));

  ScopedXI2Event xev;
  // A is not allowed on the blocked keyboard, and should return ET_UNKNOWN.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromXEvent(*xev));

  // The B key is allowed as an exception, and should return KEY_PRESSED.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::ET_KEY_PRESSED, ui::VKEY_B, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromXEvent(*xev));

  // Both A and B are allowed on an unblocked keyboard device.
  xev.InitGenericKeyEvent(master_device_id, other_device_id, ui::ET_KEY_PRESSED,
                          ui::VKEY_A, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromXEvent(*xev));
  xev.InitGenericKeyEvent(master_device_id, other_device_id, ui::ET_KEY_PRESSED,
                          ui::VKEY_B, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromXEvent(*xev));

  device_data_manager->EnableDevice(blocked_device);
  device_data_manager->SetDisabledKeyboardAllowedKeys(nullptr);

  // A key returns KEY_PRESSED as per usual now that keyboard was re-enabled.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromXEvent(*xev));
}

// Verifies that the type of events from a disabled mouse is ET_UNKNOWN.
TEST_F(EventsXTest, DisableMouse) {
  DeviceDataManagerX11* device_data_manager =
      static_cast<DeviceDataManagerX11*>(DeviceDataManager::GetInstance());
  int blocked_device_id = 1;
  auto blocked_device = static_cast<x11::Input::DeviceId>(blocked_device_id);
  int other_device_id = 2;
  std::vector<int> device_list;
  device_list.push_back(blocked_device_id);
  device_list.push_back(other_device_id);
  TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);

  device_data_manager->DisableDevice(blocked_device);

  ScopedXI2Event xev;
  xev.InitGenericButtonEvent(blocked_device_id, ET_MOUSE_PRESSED, gfx::Point(),
                             EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromXEvent(*xev));

  xev.InitGenericButtonEvent(other_device_id, ET_MOUSE_PRESSED, gfx::Point(),
                             EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromXEvent(*xev));

  device_data_manager->EnableDevice(blocked_device);

  xev.InitGenericButtonEvent(blocked_device_id, ET_MOUSE_PRESSED, gfx::Point(),
                             EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromXEvent(*xev));
}

#if !defined(OS_CHROMEOS)
TEST_F(EventsXTest, ImeFabricatedKeyEvents) {
  Display* display = gfx::GetXDisplay();

  x11::KeyButMask state_to_be_fabricated[] = {
      {},
      x11::KeyButMask::Shift,
      x11::KeyButMask::Lock,
      x11::KeyButMask::Shift | x11::KeyButMask::Lock,
  };
  for (auto state : state_to_be_fabricated) {
    for (int is_char = 0; is_char < 2; ++is_char) {
      x11::Event x_event;
      InitKeyEvent(display, &x_event, true, 0, state);
      auto key_event = ui::BuildKeyEventFromXEvent(x_event);
      if (is_char) {
        KeyEventTestApi test_event(key_event.get());
        test_event.set_is_char(true);
      }
      EXPECT_TRUE(key_event->flags() & ui::EF_IME_FABRICATED_KEY);
    }
  }

  x11::KeyButMask state_to_be_not_fabricated[] = {
      x11::KeyButMask::Control,
      x11::KeyButMask::Mod1,
      x11::KeyButMask::Mod2,
      x11::KeyButMask::Shift | x11::KeyButMask::Control,
  };
  for (auto state : state_to_be_not_fabricated) {
    for (int is_char = 0; is_char < 2; ++is_char) {
      x11::Event x_event;
      InitKeyEvent(display, &x_event, true, 0, state);
      auto key_event = ui::BuildKeyEventFromXEvent(x_event);
      if (is_char) {
        KeyEventTestApi test_event(key_event.get());
        test_event.set_is_char(true);
      }
      EXPECT_FALSE(key_event->flags() & ui::EF_IME_FABRICATED_KEY);
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
  EXPECT_EQ(ui::ET_UNKNOWN, ui::EventTypeFromXEvent(*xev));
}

namespace {

// Returns a fake TimeTicks based on the given millisecond offset.
base::TimeTicks TimeTicksFromMillis(int64_t millis) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(millis);
}

}  // namespace

TEST_F(EventsXTest, TimestampRolloverAndAdjustWhenDecreasing) {
  x11::Event event;
  InitButtonEvent(&event, true, gfx::Point(5, 10), 1, {});

  test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(TimeTicksFromMillis(0x100000001));
  ResetTimestampRolloverCountersForTesting();

  event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(0xFFFFFFFF);
  EXPECT_EQ(TimeTicksFromMillis(0xFFFFFFFF), ui::EventTimeFromXEvent(event));

  clock.SetNowTicks(TimeTicksFromMillis(0x100000007));
  ResetTimestampRolloverCountersForTesting();

  event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(3);
  EXPECT_EQ(TimeTicksFromMillis(0x100000000 + 3),
            ui::EventTimeFromXEvent(event));
}

TEST_F(EventsXTest, NoTimestampRolloverWhenMonotonicIncreasing) {
  x11::Event event;
  InitButtonEvent(&event, true, gfx::Point(5, 10), 1, {});

  test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(TimeTicksFromMillis(10));
  ResetTimestampRolloverCountersForTesting();

  event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(6);
  EXPECT_EQ(TimeTicksFromMillis(6), ui::EventTimeFromXEvent(event));
  event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(7);
  EXPECT_EQ(TimeTicksFromMillis(7), ui::EventTimeFromXEvent(event));

  clock.SetNowTicks(TimeTicksFromMillis(0x100000005));
  ResetTimestampRolloverCountersForTesting();

  event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(0xFFFFFFFF);
  EXPECT_EQ(TimeTicksFromMillis(0xFFFFFFFF), ui::EventTimeFromXEvent(event));
}

}  // namespace ui
