// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/types/event_type.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

// Initializes the passed-in event.
void InitButtonEvent(x11::Event* event,
                     bool is_press,
                     const gfx::Point& location,
                     int button,
                     x11::KeyButMask state) {
  *event = x11::Event(false, x11::ButtonEvent{
                                 .opcode = is_press ? x11::ButtonEvent::Press
                                                    : x11::ButtonEvent::Release,
                                 .detail = static_cast<x11::Button>(button),
                                 .event_x = static_cast<int16_t>(location.x()),
                                 .event_y = static_cast<int16_t>(location.y()),
                                 .state = state,
                             });
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Initializes the passed-in x11::Event.
void InitKeyEvent(x11::Event* event,
                  bool is_press,
                  int keycode,
                  x11::KeyButMask state) {
  // We don't bother setting fields that the event code doesn't use, such as
  // x_root/y_root and window/root/subwindow.
  *event = x11::Event(false, x11::KeyEvent{
                                 .opcode = is_press ? x11::KeyEvent::Press
                                                    : x11::KeyEvent::Release,
                                 .detail = static_cast<x11::KeyCode>(keycode),
                                 .state = state,
                             });
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

x11::Input::Fp1616 ToFp1616(int x) {
  return static_cast<x11::Input::Fp1616>(x * (1 << 16));
}

}  // namespace

class EventsXTest : public testing::Test {
 public:
  EventsXTest() = default;

  EventsXTest(const EventsXTest&) = delete;
  EventsXTest& operator=(const EventsXTest&) = delete;

  ~EventsXTest() override = default;

  void SetUp() override {
    DeviceDataManagerX11::CreateInstance();
    ui::TouchFactory::GetInstance()->ResetForTest();
    ResetTimestampRolloverCountersForTesting();
  }
  void TearDown() override { ResetTimestampRolloverCountersForTesting(); }
};

TEST_F(EventsXTest, ButtonEvents) {
  x11::Event event;
  gfx::Point location(5, 10);
  gfx::Vector2d offset;

  InitButtonEvent(&event, true, location, 1, {});
  EXPECT_EQ(ui::EventType::kMousePressed, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  InitButtonEvent(&event, true, location, 2,
                  x11::KeyButMask::Button1 | x11::KeyButMask::Shift);
  EXPECT_EQ(ui::EventType::kMousePressed, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
      ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  InitButtonEvent(&event, false, location, 3, {});
  EXPECT_EQ(ui::EventType::kMouseReleased, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON,
            ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));

  // Scroll up.
  InitButtonEvent(&event, true, location, 4, {});
  EXPECT_EQ(ui::EventType::kMousewheel, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  InitButtonEvent(&event, true, location, 5, {});
  EXPECT_EQ(ui::EventType::kMousewheel, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left.
  InitButtonEvent(&event, true, location, 6, {});
  EXPECT_EQ(ui::EventType::kMousewheel, ui::EventTypeFromXEvent(event));
  EXPECT_EQ(0, ui::EventFlagsFromXEvent(event));
  EXPECT_EQ(ui::EF_NONE, ui::GetChangedMouseButtonFlagsFromXEvent(event));
  EXPECT_EQ(location, ui::EventLocationFromXEvent(event));
  offset = ui::GetMouseWheelOffsetFromXEvent(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);

  // Scroll right.
  InitButtonEvent(&event, true, location, 7, {});
  EXPECT_EQ(ui::EventType::kMousewheel, ui::EventTypeFromXEvent(event));
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
  EXPECT_EQ(ui::EventType::kMousewheel, ui::EventTypeFromXEvent(event));

  // We should return EventType::kUnknown for the release event instead of
  // returning EventType::kMousewheel; otherwise we'll scroll twice for each
  // scrollwheel step.
  InitButtonEvent(&event, false, location, 4, {});
  EXPECT_EQ(ui::EventType::kUnknown, ui::EventTypeFromXEvent(event));

  // TODO(derat): Test XInput code.
}

TEST_F(EventsXTest, EnterLeaveEvent) {
  x11::Event event(false, x11::CrossingEvent{
                              .opcode = x11::CrossingEvent::EnterNotify,
                              .root_x = 110,
                              .root_y = 120,
                              .event_x = 10,
                              .event_y = 20,
                          });

  // Mouse enter events are converted to mouse move events to be consistent with
  // the way views handle mouse enter. See comments for EnterNotify case in
  // ui::EventTypeFromXEvent for more details.
  EXPECT_EQ(ui::EventType::kMouseMoved, ui::EventTypeFromXEvent(event));
  EXPECT_TRUE(ui::EventFlagsFromXEvent(event) & ui::EF_IS_SYNTHESIZED);
  EXPECT_EQ("10,20", ui::EventLocationFromXEvent(event).ToString());
  EXPECT_EQ("110,120", ui::EventSystemLocationFromXEvent(event).ToString());

  event = x11::Event(false, x11::CrossingEvent{
                                .opcode = x11::CrossingEvent::LeaveNotify,
                                .root_x = 230,
                                .root_y = 240,
                                .event_x = 30,
                                .event_y = 40,
                            });
  EXPECT_EQ(ui::EventType::kMouseExited, ui::EventTypeFromXEvent(event));
  EXPECT_EQ("30,40", ui::EventLocationFromXEvent(event).ToString());
  EXPECT_EQ("230,240", ui::EventSystemLocationFromXEvent(event).ToString());
}

TEST_F(EventsXTest, XInputEnterLeaveEvent) {
  x11::Event event(false, x11::Input::CrossingEvent{
                              .opcode = x11::Input::CrossingEvent::Enter,
                              .root_x = ToFp1616(110),
                              .root_y = ToFp1616(120),
                              .event_x = ToFp1616(10),
                              .event_y = ToFp1616(20),
                          });
  EXPECT_EQ("110,120", ui::EventSystemLocationFromXEvent(event).ToString());

  event = x11::Event(false, x11::Input::CrossingEvent{
                                .opcode = x11::Input::CrossingEvent::Leave,
                                .root_x = ToFp1616(230),
                                .root_y = ToFp1616(240),
                                .event_x = ToFp1616(30),
                                .event_y = ToFp1616(40),
                            });
  EXPECT_EQ("230,240", ui::EventSystemLocationFromXEvent(event).ToString());
}

TEST_F(EventsXTest, ClickCount) {
  x11::Event event;
  gfx::Point location(5, 10);

  base::TimeDelta time_stamp =
      base::TimeTicks::Now().since_origin() - base::Milliseconds(10);
  for (int i = 1; i <= 3; ++i) {
    InitButtonEvent(&event, true, location, 1, {});
    {
      uint32_t time = time_stamp.InMilliseconds() & UINT32_MAX;
      event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(time);
      auto mouseev = ui::BuildMouseEventFromXEvent(event);
      EXPECT_EQ(ui::EventType::kMousePressed, mouseev->type());
      EXPECT_EQ(i, mouseev->GetClickCount());
    }

    InitButtonEvent(&event, false, location, 1, {});
    {
      uint32_t time = time_stamp.InMilliseconds() & UINT32_MAX;
      event.As<x11::ButtonEvent>()->time = static_cast<x11::Time>(time);
      auto mouseev = ui::BuildMouseEventFromXEvent(event);
      EXPECT_EQ(ui::EventType::kMouseReleased, mouseev->type());
      EXPECT_EQ(i, mouseev->GetClickCount());
    }
    time_stamp += base::Milliseconds(1);
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
  EXPECT_EQ(ui::EventType::kTouchPressed,
            ui::EventTypeFromXEvent(*scoped_xevent));
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
  EXPECT_EQ(ui::EventType::kTouchMoved,
            ui::EventTypeFromXEvent(*scoped_xevent));
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
  EXPECT_EQ(ui::EventType::kTouchPressed,
            ui::EventTypeFromXEvent(*scoped_xevent));
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
  EXPECT_EQ(ui::EventType::kTouchReleased,
            ui::EventTypeFromXEvent(*scoped_xevent));
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
  EXPECT_EQ(ui::EventType::kTouchReleased,
            ui::EventTypeFromXEvent(*scoped_xevent));
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

// Verifies that the type of events from a disabled keyboard is
// EventType::kUnknown, but that an exception list of keys can still be
// processed.
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
  // A is not allowed on the blocked keyboard, and should return
  // EventType::kUnknown.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::EventType::kKeyPressed, ui::VKEY_A, 0);
  EXPECT_EQ(ui::EventType::kUnknown, ui::EventTypeFromXEvent(*xev));

  // The B key is allowed as an exception, and should return KEY_PRESSED.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::EventType::kKeyPressed, ui::VKEY_B, 0);
  EXPECT_EQ(ui::EventType::kKeyPressed, ui::EventTypeFromXEvent(*xev));

  // Both A and B are allowed on an unblocked keyboard device.
  xev.InitGenericKeyEvent(master_device_id, other_device_id,
                          ui::EventType::kKeyPressed, ui::VKEY_A, 0);
  EXPECT_EQ(ui::EventType::kKeyPressed, ui::EventTypeFromXEvent(*xev));
  xev.InitGenericKeyEvent(master_device_id, other_device_id,
                          ui::EventType::kKeyPressed, ui::VKEY_B, 0);
  EXPECT_EQ(ui::EventType::kKeyPressed, ui::EventTypeFromXEvent(*xev));

  device_data_manager->EnableDevice(blocked_device);
  device_data_manager->SetDisabledKeyboardAllowedKeys(nullptr);

  // A key returns KEY_PRESSED as per usual now that keyboard was re-enabled.
  xev.InitGenericKeyEvent(master_device_id, blocked_device_id,
                          ui::EventType::kKeyPressed, ui::VKEY_A, 0);
  EXPECT_EQ(ui::EventType::kKeyPressed, ui::EventTypeFromXEvent(*xev));
}

// Verifies that the type of events from a disabled mouse is
// EventType::kUnknown.
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
  xev.InitGenericButtonEvent(blocked_device_id, EventType::kMousePressed,
                             gfx::Point(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EventType::kUnknown, ui::EventTypeFromXEvent(*xev));

  xev.InitGenericButtonEvent(other_device_id, EventType::kMousePressed,
                             gfx::Point(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EventType::kMousePressed, ui::EventTypeFromXEvent(*xev));

  device_data_manager->EnableDevice(blocked_device);

  xev.InitGenericButtonEvent(blocked_device_id, EventType::kMousePressed,
                             gfx::Point(), EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EventType::kMousePressed, ui::EventTypeFromXEvent(*xev));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(EventsXTest, ImeFabricatedKeyEvents) {
  x11::KeyButMask state_to_be_fabricated[] = {
      {},
      x11::KeyButMask::Shift,
      x11::KeyButMask::Lock,
      x11::KeyButMask::Shift | x11::KeyButMask::Lock,
  };
  for (auto state : state_to_be_fabricated) {
    for (int is_char = 0; is_char < 2; ++is_char) {
      x11::Event x_event;
      InitKeyEvent(&x_event, true, 0, state);
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
      InitKeyEvent(&x_event, true, 0, state);
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
  EXPECT_EQ(ui::EventType::kUnknown, ui::EventTypeFromXEvent(*xev));
}

namespace {

// Returns a fake TimeTicks based on the given millisecond offset.
base::TimeTicks TimeTicksFromMillis(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
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

// Moved from event_unittest.cc

TEST_F(EventsXTest, NativeEvent) {
  ScopedXI2Event event;
  event.InitKeyEvent(EventType::kKeyReleased, VKEY_A, EF_NONE);
  auto keyev = ui::BuildKeyEventFromXEvent(*event);
  EXPECT_FALSE(keyev->HasNativeEvent());
}

TEST_F(EventsXTest, GetCharacter) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  // For X11, test the functions with native_event() as well. crbug.com/107837
  ScopedXI2Event event;
  event.InitKeyEvent(EventType::kKeyPressed, VKEY_RETURN, EF_CONTROL_DOWN);
  auto keyev3 = ui::BuildKeyEventFromXEvent(*event);
  EXPECT_EQ(10, keyev3->GetCharacter());

  event.InitKeyEvent(EventType::kKeyPressed, VKEY_RETURN, EF_NONE);
  auto keyev4 = ui::BuildKeyEventFromXEvent(*event);
  EXPECT_EQ(13, keyev4->GetCharacter());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(EventsXTest, NormalizeKeyEventFlags) {
  // Normalize flags when KeyEvent is created from XEvent.
  ScopedXI2Event event;
  {
    event.InitKeyEvent(EventType::kKeyPressed, VKEY_SHIFT, EF_SHIFT_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev->flags());
  }
  {
    event.InitKeyEvent(EventType::kKeyReleased, VKEY_SHIFT, EF_SHIFT_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_NONE, keyev->flags());
  }
  {
    event.InitKeyEvent(EventType::kKeyPressed, VKEY_CONTROL, EF_CONTROL_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev->flags());
  }
  {
    event.InitKeyEvent(EventType::kKeyReleased, VKEY_CONTROL, EF_CONTROL_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_NONE, keyev->flags());
  }
  {
    event.InitKeyEvent(EventType::kKeyPressed, VKEY_MENU, EF_ALT_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_ALT_DOWN, keyev->flags());
  }
  {
    event.InitKeyEvent(EventType::kKeyReleased, VKEY_MENU, EF_ALT_DOWN);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(EF_NONE, keyev->flags());
  }
}
#endif

TEST_F(EventsXTest, KeyEventCode) {
  const DomCode kDomCodeForSpace = DomCode::SPACE;
  const char kCodeForSpace[] = "Space";
  ASSERT_EQ(kDomCodeForSpace,
            ui::KeycodeConverter::CodeStringToDomCode(kCodeForSpace));
  const uint16_t kNativeCodeSpace =
      ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace);
  ASSERT_NE(ui::KeycodeConverter::InvalidNativeKeycode(), kNativeCodeSpace);
  ASSERT_EQ(kNativeCodeSpace,
            ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace));

  // KeyEvent converts from the native keycode (XKB) to the code.
  ScopedXI2Event xevent;
  xevent.InitKeyEvent(EventType::kKeyPressed, VKEY_SPACE, kNativeCodeSpace);
  auto keyev = ui::BuildKeyEventFromXEvent(*xevent);
  EXPECT_EQ(kCodeForSpace, keyev->GetCodeString());
}

namespace {

void SetKeyEventTimestamp(x11::Event* event, int64_t time64) {
  uint32_t time = time64 & UINT32_MAX;
  event->As<x11::KeyEvent>()->time = static_cast<x11::Time>(time);
}

void AdvanceKeyEventTimestamp(x11::Event* event) {
  auto time = static_cast<uint32_t>(event->As<x11::KeyEvent>()->time) + 1;
  event->As<x11::KeyEvent>()->time = static_cast<x11::Time>(time);
}

}  // namespace

TEST_F(EventsXTest, AutoRepeat) {
  const uint16_t kNativeCodeA =
      ui::KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_A);
  const uint16_t kNativeCodeB =
      ui::KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_B);

  ScopedXI2Event native_event_a_pressed;
  native_event_a_pressed.InitKeyEvent(EventType::kKeyPressed, VKEY_A,
                                      kNativeCodeA);
  ScopedXI2Event native_event_a_pressed_1500;
  native_event_a_pressed_1500.InitKeyEvent(EventType::kKeyPressed, VKEY_A,
                                           kNativeCodeA);
  ScopedXI2Event native_event_a_pressed_3000;
  native_event_a_pressed_3000.InitKeyEvent(EventType::kKeyPressed, VKEY_A,
                                           kNativeCodeA);

  ScopedXI2Event native_event_a_released;
  native_event_a_released.InitKeyEvent(EventType::kKeyReleased, VKEY_A,
                                       kNativeCodeA);
  ScopedXI2Event native_event_b_pressed;
  native_event_b_pressed.InitKeyEvent(EventType::kKeyPressed, VKEY_B,
                                      kNativeCodeB);
  ScopedXI2Event native_event_a_pressed_nonstandard_state;
  native_event_a_pressed_nonstandard_state.InitKeyEvent(EventType::kKeyPressed,
                                                        VKEY_A, kNativeCodeA);
  // IBUS-GTK uses the mask (1 << 25) to detect reposted event.
  {
    x11::Event& event = *native_event_a_pressed_nonstandard_state;
    int mask = static_cast<int>(event.As<x11::KeyEvent>()->state) | 1 << 25;
    event.As<x11::KeyEvent>()->state = static_cast<x11::KeyButMask>(mask);
  }

  int64_t ticks_base =
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds() - 5000;
  SetKeyEventTimestamp(native_event_a_pressed, ticks_base);
  SetKeyEventTimestamp(native_event_a_pressed_1500, ticks_base + 1500);
  SetKeyEventTimestamp(native_event_a_pressed_3000, ticks_base + 3000);

  {
    auto key_a1 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a1->is_repeat());

    auto key_a1_released = BuildKeyEventFromXEvent(*native_event_a_released);
    EXPECT_FALSE(key_a1_released->is_repeat());

    auto key_a2 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a2->is_repeat());

    AdvanceKeyEventTimestamp(native_event_a_pressed);
    auto key_a2_repeated = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_TRUE(key_a2_repeated->is_repeat());

    auto key_a2_released = BuildKeyEventFromXEvent(*native_event_a_released);
    EXPECT_FALSE(key_a2_released->is_repeat());
  }

  // Interleaved with different key press.
  {
    auto key_a3 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a3->is_repeat());

    auto key_b = BuildKeyEventFromXEvent(*native_event_b_pressed);
    EXPECT_FALSE(key_b->is_repeat());

    AdvanceKeyEventTimestamp(native_event_a_pressed);
    auto key_a3_again = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a3_again->is_repeat());

    AdvanceKeyEventTimestamp(native_event_a_pressed);
    auto key_a3_repeated = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_TRUE(key_a3_repeated->is_repeat());

    AdvanceKeyEventTimestamp(native_event_a_pressed);
    auto key_a3_repeated2 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_TRUE(key_a3_repeated2->is_repeat());

    auto key_a3_released = BuildKeyEventFromXEvent(*native_event_a_released);
    EXPECT_FALSE(key_a3_released->is_repeat());
  }

  // Hold the key longer than max auto repeat timeout.
  {
    auto key_a4_0 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a4_0->is_repeat());

    auto key_a4_1500 = BuildKeyEventFromXEvent(*native_event_a_pressed_1500);
    EXPECT_TRUE(key_a4_1500->is_repeat());

    auto key_a4_3000 = BuildKeyEventFromXEvent(*native_event_a_pressed_3000);
    EXPECT_TRUE(key_a4_3000->is_repeat());

    auto key_a4_released = BuildKeyEventFromXEvent(*native_event_a_released);
    EXPECT_FALSE(key_a4_released->is_repeat());
  }

  {
    auto key_a4_pressed = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a4_pressed->is_repeat());

    auto key_a4_pressed_nonstandard_state =
        BuildKeyEventFromXEvent(*native_event_a_pressed_nonstandard_state);
    EXPECT_FALSE(key_a4_pressed_nonstandard_state->is_repeat());
  }

  {
    auto key_a1 = BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a1->is_repeat());

    auto key_a1_with_same_event =
        BuildKeyEventFromXEvent(*native_event_a_pressed);
    EXPECT_FALSE(key_a1_with_same_event->is_repeat());
  }
}

// Checks that Event.Latency.OS2.TOUCH_PRESSED, TOUCH_MOVED,
// and TOUCH_RELEASED histograms are computed properly.
TEST_F(EventsXTest, EventLatencyOSTouchHistograms) {
  base::HistogramTester histogram_tester;
  ScopedXI2Event scoped_xevent;

  // SetUp for test
  DeviceDataManagerX11::CreateInstance();
  std::vector<int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);

  // Init touch begin, update, and end events with tracking id 5, touch id 0.
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchBegin, 5,
                               gfx::Point(10, 10), {});
  auto touch_begin = ui::BuildTouchEventFromXEvent(*scoped_xevent);
  histogram_tester.ExpectTotalCount("Event.Latency.OS2.TOUCH_PRESSED", 1);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchUpdate, 5,
                               gfx::Point(20, 20), {});
  auto touch_update = ui::BuildTouchEventFromXEvent(*scoped_xevent);
  histogram_tester.ExpectTotalCount("Event.Latency.OS2.TOUCH_MOVED", 1);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchEnd, 5,
                               gfx::Point(30, 30), {});
  auto touch_end = ui::BuildTouchEventFromXEvent(*scoped_xevent);
  histogram_tester.ExpectTotalCount("Event.Latency.OS2.TOUCH_RELEASED", 1);
}

TEST_F(EventsXTest, EventLatencyOSMouseWheelHistogram) {
  base::HistogramTester histogram_tester;
  DeviceDataManagerX11::CreateInstance();

  // Initializes a native event and uses it to generate a MouseWheel event.
  x11::Event native_event(
      false, x11::ButtonEvent{
                 .opcode = x11::ButtonEvent::Press,
                 // A valid wheel button number between min and max.
                 .detail = static_cast<x11::Button>(4),
             });
  auto mouse_ev = ui::BuildMouseWheelEventFromXEvent(native_event);
  histogram_tester.ExpectTotalCount("Event.Latency.OS2.MOUSE_WHEEL", 1);
}

}  // namespace ui
