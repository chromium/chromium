// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include <xcb/xcb.h>

#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

int XkbBuildCoreState(int key_button_mask, int group) {
  return ((group & 0x3) << 13) | (key_button_mask & 0xff);
}

}  // namespace

// Ensure DomKey extraction happens lazily in ash-chrome (ie: linux-chromeos),
// while in Linux Desktop path it is set right away in XEvent => ui::Event
// translation and it's properly copied when native event ctor is used. This
// prevents regressions such as crbug.com/1007389 and crbug.com/1240616.
TEST(XEventTranslationTest, KeyEventDomKeyExtraction) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event xev;
  xev.InitKeyEvent(EventType::kKeyPressed, VKEY_RETURN, EF_NONE);

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  KeyEventTestApi test(keyev.get());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(ui::DomKey::NONE, test.dom_key());
#else
  EXPECT_EQ(ui::DomKey::ENTER, test.dom_key());
#endif

  EXPECT_EQ(13, keyev->GetCharacter());
  EXPECT_EQ("Enter", keyev->GetCodeString());

  KeyEvent copy(keyev.get());
  EXPECT_EQ(ui::DomKey::ENTER, KeyEventTestApi(&copy).dom_key());
  EXPECT_EQ(ui::DomKey::ENTER, copy.GetDomKey());
}

// Ensure KeyEvent::Properties is properly set regardless X11 build config is
// in place. This prevents regressions such as crbug.com/1047999.
TEST(XEventTranslationTest, KeyEventXEventPropertiesSet) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event scoped_xev;
  scoped_xev.InitKeyEvent(EventType::kKeyPressed, VKEY_A, EF_NONE);

  x11::Event* xev = scoped_xev;
  auto* connection = x11::Connection::Get();
  // Set keyboard group in XKeyEvent
  uint32_t state = XkbBuildCoreState(
      static_cast<uint32_t>(xev->As<x11::KeyEvent>()->state), 2u);
  // Set IME-specific flags
  state |= 0x3 << ui::kPropertyKeyboardImeFlagOffset;
  xev->As<x11::KeyEvent>()->state = static_cast<x11::KeyButMask>(state);

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  auto* properties = keyev->properties();
  EXPECT_TRUE(properties);
  EXPECT_EQ(4u, properties->size());

  // Ensure hardware keycode, keyboard group and IME flag properties are
  // properly set.
  auto state_it = properties->find(ui::kPropertyKeyboardState);
  ASSERT_NE(state_it, properties->end());
  EXPECT_EQ(4u, state_it->second.size());
  // Making sure the value is stored in little endian.
  EXPECT_EQ(static_cast<uint8_t>(state), state_it->second[0]);
  EXPECT_EQ(static_cast<uint8_t>(state >> 8), state_it->second[1]);
  EXPECT_EQ(static_cast<uint8_t>(state >> 16), state_it->second[2]);
  EXPECT_EQ(static_cast<uint8_t>(state >> 24), state_it->second[3]);

  auto hw_keycode_it = properties->find(ui::kPropertyKeyboardHwKeyCode);
  EXPECT_NE(hw_keycode_it, properties->end());
  EXPECT_EQ(1u, hw_keycode_it->second.size());
  EXPECT_EQ(static_cast<uint8_t>(connection->KeysymToKeycode(XK_a)),
            hw_keycode_it->second[0]);

  auto kbd_group_it = properties->find(ui::kPropertyKeyboardGroup);
  EXPECT_NE(kbd_group_it, properties->end());
  EXPECT_EQ(1u, kbd_group_it->second.size());
  EXPECT_EQ(2u, kbd_group_it->second[0]);

  auto ime_flag_it = properties->find(ui::kPropertyKeyboardImeFlag);
  EXPECT_NE(ime_flag_it, properties->end());
  EXPECT_EQ(1u, ime_flag_it->second.size());
  EXPECT_EQ(0x3, ime_flag_it->second[0]);
}

// Ensure XEvents with bogus timestamps are properly handled when translated
// into ui::*Events.
TEST(XEventTranslationTest, BogusTimestampCorrection) {
  using base::TimeTicks;

  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event scoped_xev;
  scoped_xev.InitKeyEvent(EventType::kKeyPressed, VKEY_RETURN, EF_NONE);
  x11::Event* xev = scoped_xev;

  test::ScopedEventTestTickClock test_clock;
  test_clock.Advance(base::Seconds(1));

  // Set initial time as 1000ms
  TimeTicks now_ticks = EventTimeForNow();
  int64_t now_ms = (now_ticks - TimeTicks()).InMilliseconds();
  EXPECT_EQ(1000, now_ms);

  // Emulate XEvent generated 500ms before current time (non-bogus) and verify
  // the translated Event uses native event's timestamp.
  xev->As<x11::KeyEvent>()->time = static_cast<x11::Time>(500);
  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);
  EXPECT_EQ(now_ticks - base::Milliseconds(500), keyev->time_stamp());

  // Emulate XEvent generated 1000ms ahead in time (bogus timestamp) and verify
  // the translated Event's timestamp is fixed using (i.e: EventTimeForNow()
  // instead of the original XEvent's time)
  xev->As<x11::KeyEvent>()->time = static_cast<x11::Time>(2000);
  auto keyev2 = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev2);
  EXPECT_EQ(EventTimeForNow(), keyev2->time_stamp());

  // Emulate XEvent >= 60sec old (bogus timestamp) and ensure translated
  // ui::Event's timestamp has been corrected (i.e: use ui::EventTimeForNow()
  // instead of the original XEvent's time). To emulate such scenario, we
  // advance the clock by 5 minutes and set the XEvent's time to 1min, so delta
  // is 4min 1sec.
  test_clock.Advance(base::Minutes(5));
  xev->As<x11::KeyEvent>()->time = static_cast<x11::Time>(1000 * 60);
  auto keyev3 = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev3);
  EXPECT_EQ(EventTimeForNow(), keyev3->time_stamp());
}

// Ensure MouseEvent::changed_button_flags is correctly translated from
// X{Button,Crossing}Events.
TEST(XEventTranslationTest, ChangedMouseButtonFlags) {
  ui::ScopedXI2Event event;
  // Taking in a ButtonPress XEvent, with left button pressed.
  event.InitButtonEvent(ui::EventType::kMousePressed, gfx::Point(500, 500),
                        ui::EF_LEFT_MOUSE_BUTTON);
  auto mouseev = ui::BuildMouseEventFromXEvent(*event);
  EXPECT_TRUE(mouseev);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, mouseev->changed_button_flags());

  // Taking in a ButtonPress XEvent, with no button pressed.
  x11::Event& x11_event = *event;
  x11_event.As<x11::ButtonEvent>()->detail = static_cast<x11::Button>(0);
  auto mouseev2 = ui::BuildMouseEventFromXEvent(*event);
  EXPECT_TRUE(mouseev2);
  EXPECT_EQ(0, mouseev2->changed_button_flags());

  // Taking in a EnterNotify XEvent
  x11::Event enter_event(false, x11::CrossingEvent{
                                    .opcode = x11::CrossingEvent::EnterNotify,
                                    .detail = x11::NotifyDetail::Virtual,
                                });

  auto mouseev3 = ui::BuildMouseEventFromXEvent(enter_event);
  EXPECT_TRUE(mouseev3);
  EXPECT_EQ(0, mouseev3->changed_button_flags());
}

// Verifies 'repeat' flag is properly set when key events for modifiers and
// their counterparts are mixed. Ensures regressions like crbug.com/1069690
// are not reintroduced in the future.
TEST(XEventTranslationTest, KeyModifiersCounterpartRepeat) {
  // Use a TestTickClock so we have the power to control the time :)
  test::ScopedEventTestTickClock test_clock;

  // Create and init a XEvent for ShiftLeft key.
  ui::ScopedXI2Event shift_l_pressed;
  shift_l_pressed.InitKeyEvent(EventType::kKeyPressed, VKEY_LSHIFT, EF_NONE);

  // Press ShiftLeft a first time and hold it.
  auto keyev_shift_l_pressed = BuildKeyEventFromXEvent(*shift_l_pressed);
  EXPECT_FALSE(keyev_shift_l_pressed->is_repeat());

  // Create a few more ShiftLeft key events and ensure 'repeat' flag is set.
  test_clock.Advance(base::Milliseconds(100));
  keyev_shift_l_pressed = BuildKeyEventFromXEvent(*shift_l_pressed);
  EXPECT_TRUE(keyev_shift_l_pressed->is_repeat());

  test_clock.Advance(base::Milliseconds(200));
  keyev_shift_l_pressed = BuildKeyEventFromXEvent(*shift_l_pressed);
  EXPECT_TRUE(keyev_shift_l_pressed->is_repeat());

  test_clock.Advance(base::Milliseconds(500));
  keyev_shift_l_pressed = BuildKeyEventFromXEvent(*shift_l_pressed);
  EXPECT_TRUE(keyev_shift_l_pressed->is_repeat());

  // Press and release ShiftRight and verify 'repeat' flag is not set.

  // Create and init XEvent for emulating a ShiftRight key press.
  ui::ScopedXI2Event shift_r_pressed;
  shift_r_pressed.InitKeyEvent(EventType::kKeyPressed, VKEY_RSHIFT,
                               EF_SHIFT_DOWN);

  test_clock.Advance(base::Seconds(1));
  auto keyev_shift_r_pressed = BuildKeyEventFromXEvent(*shift_r_pressed);
  EXPECT_FALSE(keyev_shift_r_pressed->is_repeat());
  EXPECT_EQ(EventType::kKeyPressed, keyev_shift_r_pressed->type());

  // Create and init XEvent for emulating a ShiftRight key release.
  ui::ScopedXI2Event shift_r_released;
  shift_r_released.InitKeyEvent(EventType::kKeyReleased, VKEY_RSHIFT,
                                EF_SHIFT_DOWN);

  test_clock.Advance(base::Milliseconds(300));
  auto keyev_shift_r_released = BuildKeyEventFromXEvent(*shift_r_released);
  EXPECT_FALSE(keyev_shift_r_released->is_repeat());
  EXPECT_EQ(EventType::kKeyReleased, keyev_shift_r_released->type());
}

// Verifies that scroll events remain EventType::kScroll type or are translated
// to EventType::kScrollFlingStart depending on their X and Y offsets.
TEST(XEventTranslationTest, ScrollEventType) {
  int device_id = 1;
  ui::SetUpTouchPadForTest(device_id);

  struct ScrollEventTestData {
    int x_offset_;
    int y_offset_;
    int x_offset_ordinal_;
    int y_offset_ordinal_;
    EventType expectedEventType_;
  };
  const std::vector<ScrollEventTestData> test_data = {
      // Ordinary horizontal scrolling remains EventType::kScroll.
      {1, 0, 1, 0, EventType::kScroll},
      // Ordinary vertical scrolling remains EventType::kScroll.
      {0, 10, 0, 10, EventType::kScroll},
      // Ordinary diagonal scrolling remains EventType::kScroll.
      {47, -11, 47, -11, EventType::kScroll},
      // If x_offset and y_offset both are 0, expected event type is
      // EventType::kScrollFlingStart and not EventType::kScroll.
      {0, 0, 0, 0, EventType::kScrollFlingStart}};

  for (const auto& data : test_data) {
    ui::ScopedXI2Event xev;
    xev.InitScrollEvent(device_id, data.x_offset_, data.y_offset_,
                        data.x_offset_ordinal_, data.y_offset_ordinal_, 2);

    const auto event = BuildEventFromXEvent(*xev);
    EXPECT_TRUE(event);
    EXPECT_EQ(event->type(), data.expectedEventType_);

    const ScrollEvent* scroll_event = static_cast<ScrollEvent*>(event.get());
    EXPECT_EQ(scroll_event->x_offset(), data.x_offset_);
    EXPECT_EQ(scroll_event->y_offset(), data.y_offset_);
    EXPECT_EQ(scroll_event->x_offset_ordinal(), data.x_offset_ordinal_);
    EXPECT_EQ(scroll_event->y_offset_ordinal(), data.y_offset_ordinal_);
  }
}
}  // namespace ui
