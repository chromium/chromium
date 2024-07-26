// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/event.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/test/test_event_target.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/win/events_win_utils.h"
#endif

namespace ui {

TEST(EventTest, NoNativeEvent) {
  KeyEvent keyev(EventType::kKeyPressed, VKEY_SPACE, EF_NONE);
  EXPECT_FALSE(keyev.HasNativeEvent());
}

TEST(EventTest, NativeEvent) {
#if BUILDFLAG(IS_WIN)
  CHROME_MSG native_event = {nullptr, WM_KEYUP, VKEY_A, 0};
  KeyEvent keyev(native_event);
  EXPECT_TRUE(keyev.HasNativeEvent());
#endif
}

TEST(EventTest, GetCharacter) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  // Check if Control+Enter returns 10.
  KeyEvent keyev1(EventType::kKeyPressed, VKEY_RETURN, EF_CONTROL_DOWN);
  EXPECT_EQ(10, keyev1.GetCharacter());
  // Check if Enter returns 13.
  KeyEvent keyev2(EventType::kKeyPressed, VKEY_RETURN, EF_NONE);
  EXPECT_EQ(13, keyev2.GetCharacter());

  // Check if expected Unicode character was returned for a key combination
  // contains Control.
  // e.g. Control+Shift+2 produces U+200C on "Persian" keyboard.
  // http://crbug.com/582453
  KeyEvent keyev5 = ui::KeyEvent::FromCharacter(
      0x200C, VKEY_UNKNOWN, ui::DomCode::NONE, EF_CONTROL_DOWN | EF_SHIFT_DOWN);
  EXPECT_EQ(0x200C, keyev5.GetCharacter());
}

TEST(EventTest, ClickCount) {
  const gfx::Point origin(0, 0);
  MouseEvent mouseev(EventType::kMousePressed, origin, origin,
                     EventTimeForNow(), 0, 0);
  for (int i = 1; i <= 3; ++i) {
    mouseev.SetClickCount(i);
    EXPECT_EQ(i, mouseev.GetClickCount());
  }
}

TEST(EventTest, RepeatedClick) {
  const gfx::Point origin(0, 0);
  MouseEvent event1(EventType::kMousePressed, origin, origin, EventTimeForNow(),
                    0, 0);
  MouseEvent event2(EventType::kMousePressed, origin, origin, EventTimeForNow(),
                    0, 0);
  LocatedEventTestApi test_event1(&event1);
  LocatedEventTestApi test_event2(&event2);

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks soon = start + base::Milliseconds(1);
  base::TimeTicks later = start + base::Milliseconds(1000);

  // Same time stamp (likely the same native event).
  test_event1.set_location(gfx::Point(0, 0));
  test_event2.set_location(gfx::Point(1, 0));
  test_event1.set_time_stamp(start);
  test_event2.set_time_stamp(start);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(event1, event2));
  MouseEvent mouse_ev3(event1);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(event1, mouse_ev3));

  // Close point.
  test_event1.set_location(gfx::Point(0, 0));
  test_event2.set_location(gfx::Point(1, 0));
  test_event1.set_time_stamp(start);
  test_event2.set_time_stamp(soon);
  EXPECT_TRUE(MouseEvent::IsRepeatedClickEvent(event1, event2));

  // Too far.
  test_event1.set_location(gfx::Point(0, 0));
  test_event2.set_location(gfx::Point(10, 0));
  test_event1.set_time_stamp(start);
  test_event2.set_time_stamp(soon);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(event1, event2));

  // Too long a time between clicks.
  test_event1.set_location(gfx::Point(0, 0));
  test_event2.set_location(gfx::Point(0, 0));
  test_event1.set_time_stamp(start);
  test_event2.set_time_stamp(later);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(event1, event2));
}

// Automatic repeat flag setting is disabled on Lacros,
// because the repeated event is generated inside ui/ozone/platform/wayland
// and reliable.
TEST(EventTest, RepeatedKeyEvent) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks time1 = start + base::Milliseconds(1);
  base::TimeTicks time2 = start + base::Milliseconds(2);
  base::TimeTicks time3 = start + base::Milliseconds(3);

  KeyEvent event1(EventType::kKeyPressed, VKEY_A, 0, start);
  KeyEvent event2(EventType::kKeyPressed, VKEY_A, 0, time1);
  KeyEvent event3(EventType::kKeyPressed, VKEY_A, EF_LEFT_MOUSE_BUTTON, time2);
  KeyEvent event4(EventType::kKeyPressed, VKEY_A, 0, time3);

  event1.InitializeNative();
  EXPECT_EQ(event1.flags() & EF_IS_REPEAT, 0);
  event2.InitializeNative();
  EXPECT_NE(event2.flags() & EF_IS_REPEAT, 0);

  event3.InitializeNative();
  EXPECT_NE(event3.flags() & EF_IS_REPEAT, 0);

  event4.InitializeNative();
  EXPECT_NE(event4.flags() & EF_IS_REPEAT, 0);
}

TEST(EventTest, NoRepeatedKeyEvent) {
  // Temporarily set the global synthesize_key_repeat_enabled to false.
  absl::Cleanup scoped_restore_settings =
      [old_value = KeyEvent::IsSynthesizeKeyRepeatEnabled()] {
        KeyEvent::SetSynthesizeKeyRepeatEnabled(old_value);
      };
  KeyEvent::SetSynthesizeKeyRepeatEnabled(false);

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks time1 = start + base::Milliseconds(1);
  base::TimeTicks time2 = start + base::Milliseconds(2);
  base::TimeTicks time3 = start + base::Milliseconds(3);

  KeyEvent event1(EventType::kKeyPressed, VKEY_A, 0, start);
  KeyEvent event2(EventType::kKeyPressed, VKEY_A, 0, time1);
  KeyEvent event3(EventType::kKeyPressed, VKEY_A, EF_LEFT_MOUSE_BUTTON, time2);
  KeyEvent event4(EventType::kKeyPressed, VKEY_A, 0, time3);

  event1.InitializeNative();
  EXPECT_EQ(event1.flags() & EF_IS_REPEAT, 0);
  event2.InitializeNative();
  EXPECT_EQ(event2.flags() & EF_IS_REPEAT, 0);

  event3.InitializeNative();
  EXPECT_EQ(event3.flags() & EF_IS_REPEAT, 0);

  event4.InitializeNative();
  EXPECT_EQ(event4.flags() & EF_IS_REPEAT, 0);
}

// Tests that re-processing the same mouse press event (detected by timestamp)
// does not yield a double click event: http://crbug.com/389162
TEST(EventTest, DoubleClickRequiresUniqueTimestamp) {
  const gfx::Point point(0, 0);
  base::TimeTicks time1 = base::TimeTicks::Now();
  base::TimeTicks time2 = time1 + base::Milliseconds(1);

  // Re-processing the same press doesn't yield a double-click.
  MouseEvent event(EventType::kMousePressed, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  // Processing a press with the same timestamp doesn't yield a double-click.
  event = MouseEvent(EventType::kMousePressed, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  // Processing a press with a later timestamp does yield a double-click.
  event = MouseEvent(EventType::kMousePressed, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();

  // Test processing a double press and release sequence with one timestamp.
  event = MouseEvent(EventType::kMousePressed, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMouseReleased, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMousePressed, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMouseReleased, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();

  // Test processing a double press and release sequence with two timestamps.
  event = MouseEvent(EventType::kMousePressed, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMouseReleased, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMousePressed, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMouseReleased, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();
}

// Tests that right clicking, then left clicking does not yield double clicks.
TEST(EventTest, SingleClickRightLeft) {
  const gfx::Point point(0, 0);
  base::TimeTicks time1 = base::TimeTicks::Now();
  base::TimeTicks time2 = time1 + base::Milliseconds(1);
  base::TimeTicks time3 = time1 + base::Milliseconds(2);

  MouseEvent event(EventType::kMousePressed, point, point, time1,
                   ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMousePressed, point, point, time2,
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMouseReleased, point, point, time2,
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(EventType::kMousePressed, point, point, time3,
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();
}

TEST(EventTest, KeyEvent) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  static const struct {
    KeyboardCode key_code;
    int flags;
    uint16_t character;
  } kTestData[] = {
      {VKEY_A, 0, 'a'},
      {VKEY_A, EF_SHIFT_DOWN, 'A'},
      {VKEY_A, EF_CAPS_LOCK_ON, 'A'},
      {VKEY_A, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON, 'a'},
      {VKEY_A, EF_CONTROL_DOWN, 0x01},
      {VKEY_A, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x01'},
      {VKEY_Z, 0, 'z'},
      {VKEY_Z, EF_SHIFT_DOWN, 'Z'},
      {VKEY_Z, EF_CAPS_LOCK_ON, 'Z'},
      {VKEY_Z, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON, 'z'},
      {VKEY_Z, EF_CONTROL_DOWN, '\x1A'},
      {VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1A'},

      {VKEY_2, EF_CONTROL_DOWN, '\x12'},
      {VKEY_2, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0'},
      {VKEY_6, EF_CONTROL_DOWN, '\x16'},
      {VKEY_6, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1E'},
      {VKEY_OEM_MINUS, EF_CONTROL_DOWN, '\x0D'},
      {VKEY_OEM_MINUS, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1F'},
      {VKEY_OEM_4, EF_CONTROL_DOWN, '\x1B'},
      {VKEY_OEM_4, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1B'},
      {VKEY_OEM_5, EF_CONTROL_DOWN, '\x1C'},
      {VKEY_OEM_5, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1C'},
      {VKEY_OEM_6, EF_CONTROL_DOWN, '\x1D'},
      {VKEY_OEM_6, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1D'},
      {VKEY_RETURN, EF_CONTROL_DOWN, '\x0A'},

      {VKEY_0, 0, '0'},
      {VKEY_0, EF_SHIFT_DOWN, ')'},
      {VKEY_0, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON, ')'},
      {VKEY_0, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x09'},

      {VKEY_9, 0, '9'},
      {VKEY_9, EF_SHIFT_DOWN, '('},
      {VKEY_9, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON, '('},
      {VKEY_9, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x08'},

      {VKEY_NUMPAD0, EF_CONTROL_DOWN, '\x10'},
      {VKEY_NUMPAD0, EF_SHIFT_DOWN, '0'},

      {VKEY_NUMPAD9, EF_CONTROL_DOWN, '\x19'},
      {VKEY_NUMPAD9, EF_SHIFT_DOWN, '9'},

      {VKEY_TAB, EF_NONE, '\t'},
      {VKEY_TAB, EF_CONTROL_DOWN, '\t'},
      {VKEY_TAB, EF_SHIFT_DOWN, '\t'},

      {VKEY_MULTIPLY, EF_CONTROL_DOWN, '\x0A'},
      {VKEY_MULTIPLY, EF_SHIFT_DOWN, '*'},
      {VKEY_ADD, EF_CONTROL_DOWN, '\x0B'},
      {VKEY_ADD, EF_SHIFT_DOWN, '+'},
      {VKEY_SUBTRACT, EF_CONTROL_DOWN, '\x0D'},
      {VKEY_SUBTRACT, EF_SHIFT_DOWN, '-'},
      {VKEY_DECIMAL, EF_CONTROL_DOWN, '\x0E'},
      {VKEY_DECIMAL, EF_SHIFT_DOWN, '.'},
      {VKEY_DIVIDE, EF_CONTROL_DOWN, '\x0F'},
      {VKEY_DIVIDE, EF_SHIFT_DOWN, '/'},

      {VKEY_OEM_1, EF_CONTROL_DOWN, '\x1B'},
      {VKEY_OEM_1, EF_SHIFT_DOWN, ':'},
      {VKEY_OEM_PLUS, EF_CONTROL_DOWN, '\x1D'},
      {VKEY_OEM_PLUS, EF_SHIFT_DOWN, '+'},
      {VKEY_OEM_COMMA, EF_CONTROL_DOWN, '\x0C'},
      {VKEY_OEM_COMMA, EF_SHIFT_DOWN, '<'},
      {VKEY_OEM_PERIOD, EF_CONTROL_DOWN, '\x0E'},
      {VKEY_OEM_PERIOD, EF_SHIFT_DOWN, '>'},
      {VKEY_OEM_3, EF_CONTROL_DOWN, '\x0'},
      {VKEY_OEM_3, EF_SHIFT_DOWN, '~'},
  };

  for (size_t i = 0; i < std::size(kTestData); ++i) {
    KeyEvent key(EventType::kKeyPressed, kTestData[i].key_code,
                 kTestData[i].flags);
    EXPECT_EQ(kTestData[i].character, key.GetCharacter())
        << " Index:" << i << " key_code:" << kTestData[i].key_code;
  }
}

TEST(EventTest, KeyEventDirectUnicode) {
  KeyEvent key = ui::KeyEvent::FromCharacter(0x1234U, ui::VKEY_UNKNOWN,
                                             ui::DomCode::NONE, ui::EF_NONE);
  EXPECT_EQ(0x1234U, key.GetCharacter());
  EXPECT_EQ(EventType::kKeyPressed, key.type());
  EXPECT_TRUE(key.is_char());
}

TEST(EventTest, NormalizeKeyEventFlags) {
  // Do not normalize flags for synthesized events without
  // KeyEvent::NormalizeFlags called explicitly.
  {
    KeyEvent keyev(EventType::kKeyPressed, VKEY_SHIFT, EF_SHIFT_DOWN);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(EventType::kKeyReleased, VKEY_SHIFT, EF_SHIFT_DOWN);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(EventType::kKeyPressed, VKEY_CONTROL, EF_CONTROL_DOWN);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(EventType::kKeyReleased, VKEY_CONTROL, EF_CONTROL_DOWN);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(EventType::kKeyPressed, VKEY_MENU, EF_ALT_DOWN);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(EventType::kKeyReleased, VKEY_MENU, EF_ALT_DOWN);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
}

TEST(EventTest, KeyEventCopy) {
  KeyEvent key(EventType::kKeyPressed, VKEY_A, EF_NONE);
  std::unique_ptr<KeyEvent> copied_key(new KeyEvent(key));
  EXPECT_EQ(copied_key->type(), key.type());
  EXPECT_EQ(copied_key->key_code(), key.key_code());
}

TEST(EventTest, KeyEventCode) {
  const DomCode kDomCodeForSpace = DomCode::SPACE;
  const char kCodeForSpace[] = "Space";
  ASSERT_EQ(kDomCodeForSpace,
            ui::KeycodeConverter::CodeStringToDomCode(kCodeForSpace));
  const int kNativeCodeSpace =
      ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace);
  ASSERT_NE(ui::KeycodeConverter::InvalidNativeKeycode(), kNativeCodeSpace);
  ASSERT_EQ(kNativeCodeSpace,
            ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace));

  {
    KeyEvent key(EventType::kKeyPressed, VKEY_SPACE, kDomCodeForSpace, EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
  {
    // Regardless the KeyEvent.key_code (VKEY_RETURN), code should be
    // the specified value.
    KeyEvent key(EventType::kKeyPressed, VKEY_RETURN, kDomCodeForSpace,
                 EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
  {
    // If the synthetic event is initialized without code, the code is
    // determined from the KeyboardCode assuming a US keyboard layout.
    KeyEvent key(EventType::kKeyPressed, VKEY_SPACE, EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
#if BUILDFLAG(IS_WIN)
  {
    // Test a non extended key.
    ASSERT_EQ((kNativeCodeSpace & 0xFF), kNativeCodeSpace);

    const LPARAM lParam = GetLParamFromScanCode(kNativeCodeSpace);
    CHROME_MSG native_event = {nullptr, WM_KEYUP, VKEY_SPACE, lParam};
    KeyEvent key(native_event);

    // KeyEvent converts from the native keycode (scan code) to the code.
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
  {
    const char kCodeForHome[] = "Home";
    const uint16_t kNativeCodeHome = 0xe047;

    // 'Home' is an extended key with 0xe000 bits.
    ASSERT_NE((kNativeCodeHome & 0xFF), kNativeCodeHome);
    const LPARAM lParam = GetLParamFromScanCode(kNativeCodeHome);

    CHROME_MSG native_event = {nullptr, WM_KEYUP, VKEY_HOME, lParam};
    KeyEvent key(native_event);

    // KeyEvent converts from the native keycode (scan code) to the code.
    EXPECT_EQ(kCodeForHome, key.GetCodeString());
  }
#endif  // BUILDFLAG(IS_WIN)
}

TEST(EventTest, TouchEventRadiusDefaultsToOtherAxis) {
  const base::TimeTicks time = base::TimeTicks::Now();
  const float non_zero_length1 = 30;
  const float non_zero_length2 = 46;

  TouchEvent event1(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                    PointerDetails(ui::EventPointerType::kTouch,
                                   /* pointer_id*/ 0,
                                   /* radius_x */ non_zero_length1,
                                   /* radius_y */ 0.0f,
                                   /* force */ 0));
  EXPECT_EQ(non_zero_length1, event1.pointer_details().radius_x);
  EXPECT_EQ(non_zero_length1, event1.pointer_details().radius_y);

  TouchEvent event2(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                    PointerDetails(ui::EventPointerType::kTouch,
                                   /* pointer_id*/ 0,
                                   /* radius_x */ 0.0f,
                                   /* radius_y */ non_zero_length2,
                                   /* force */ 0));
  EXPECT_EQ(non_zero_length2, event2.pointer_details().radius_x);
  EXPECT_EQ(non_zero_length2, event2.pointer_details().radius_y);
}

TEST(EventTest, TouchEventRotationAngleFixing) {
  const base::TimeTicks time = base::TimeTicks::Now();
  const float radius_x = 20;
  const float radius_y = 10;

  {
    const float angle_in_range = 0;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_in_range),
                     0);
    EXPECT_FLOAT_EQ(angle_in_range, event.ComputeRotationAngle());
  }

  {
    const float angle_in_range = 179.9f;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_in_range),
                     0);
    EXPECT_FLOAT_EQ(angle_in_range, event.ComputeRotationAngle());
  }

  {
    const float angle_negative = -0.1f;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_negative),
                     0);
    EXPECT_FLOAT_EQ(180 - 0.1f, event.ComputeRotationAngle());
  }

  {
    const float angle_negative = -200;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_negative),
                     0);
    EXPECT_FLOAT_EQ(360 - 200, event.ComputeRotationAngle());
  }

  {
    const float angle_too_big = 180;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_too_big),
                     0);
    EXPECT_FLOAT_EQ(0, event.ComputeRotationAngle());
  }

  {
    const float angle_too_big = 400;
    TouchEvent event(ui::EventType::kTouchPressed, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_too_big),
                     0);
    EXPECT_FLOAT_EQ(400 - 360, event.ComputeRotationAngle());
  }
}

TEST(EventTest, PointerDetailsTouch) {
  ui::TouchEvent touch_event_plain(
      EventType::kTouchPressed, gfx::Point(0, 0), ui::EventTimeForNow(),
      PointerDetails(ui::EventPointerType::kTouch, 0));

  EXPECT_EQ(EventPointerType::kTouch,
            touch_event_plain.pointer_details().pointer_type);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().radius_x);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().radius_y);
  EXPECT_TRUE(std::isnan(touch_event_plain.pointer_details().force));
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().tilt_x);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().tilt_y);

  ui::TouchEvent touch_event_with_details(
      EventType::kTouchPressed, gfx::Point(0, 0), ui::EventTimeForNow(),
      PointerDetails(ui::EventPointerType::kTouch,
                     /* pointer_id*/ 0,
                     /* radius_x */ 10.0f,
                     /* radius_y */ 5.0f,
                     /* force */ 15.0f));

  EXPECT_EQ(EventPointerType::kTouch,
            touch_event_with_details.pointer_details().pointer_type);
  EXPECT_EQ(10.0f, touch_event_with_details.pointer_details().radius_x);
  EXPECT_EQ(5.0f, touch_event_with_details.pointer_details().radius_y);
  EXPECT_EQ(15.0f, touch_event_with_details.pointer_details().force);
  EXPECT_EQ(0.0f, touch_event_with_details.pointer_details().tilt_x);
  EXPECT_EQ(0.0f, touch_event_with_details.pointer_details().tilt_y);

  ui::TouchEvent touch_event_copy(touch_event_with_details);
  EXPECT_EQ(touch_event_with_details.pointer_details(),
            touch_event_copy.pointer_details());
}

TEST(EventTest, PointerDetailsMouse) {
  ui::MouseEvent mouse_event(EventType::kMousePressed, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  EXPECT_EQ(EventPointerType::kMouse,
            mouse_event.pointer_details().pointer_type);
  EXPECT_EQ(0.0f, mouse_event.pointer_details().radius_x);
  EXPECT_EQ(0.0f, mouse_event.pointer_details().radius_y);
  EXPECT_TRUE(std::isnan(mouse_event.pointer_details().force));
  EXPECT_EQ(0.0f, mouse_event.pointer_details().tilt_x);
  EXPECT_EQ(0.0f, mouse_event.pointer_details().tilt_y);

  ui::MouseEvent mouse_event_copy(mouse_event);
  EXPECT_EQ(mouse_event.pointer_details(), mouse_event_copy.pointer_details());
}

TEST(EventTest, PointerDetailsStylus) {
  ui::PointerDetails pointer_details(EventPointerType::kPen,
                                     /* pointer_id*/ 0,
                                     /* radius_x */ 0.0f,
                                     /* radius_y */ 0.0f,
                                     /* force */ 21.0f,
                                     /* twist */ 196,
                                     /* tilt_x */ 45.0f,
                                     /* tilt_y */ -45.0f,
                                     /* tangential_pressure */ 0.7f);

  ui::MouseEvent stylus_event(EventType::kMousePressed, gfx::Point(0, 0),
                              gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0,
                              pointer_details);
  EXPECT_EQ(EventPointerType::kPen,
            stylus_event.pointer_details().pointer_type);
  EXPECT_EQ(21.0f, stylus_event.pointer_details().force);
  EXPECT_EQ(45.0f, stylus_event.pointer_details().tilt_x);
  EXPECT_EQ(-45.0f, stylus_event.pointer_details().tilt_y);
  EXPECT_EQ(0.0f, stylus_event.pointer_details().radius_x);
  EXPECT_EQ(0.0f, stylus_event.pointer_details().radius_y);
  EXPECT_EQ(0.7f, stylus_event.pointer_details().tangential_pressure);
  EXPECT_EQ(196, stylus_event.pointer_details().twist);

  ui::MouseEvent stylus_event_copy(stylus_event);
  EXPECT_EQ(stylus_event.pointer_details(),
            stylus_event_copy.pointer_details());
}

TEST(EventTest, PointerDetailsCustomTouch) {
  ui::TouchEvent touch_event(EventType::kTouchPressed, gfx::Point(0, 0),
                             ui::EventTimeForNow(),
                             PointerDetails(ui::EventPointerType::kTouch, 0));

  EXPECT_EQ(EventPointerType::kTouch,
            touch_event.pointer_details().pointer_type);
  EXPECT_EQ(0.0f, touch_event.pointer_details().radius_x);
  EXPECT_EQ(0.0f, touch_event.pointer_details().radius_y);
  EXPECT_TRUE(std::isnan(touch_event.pointer_details().force));
  EXPECT_EQ(0.0f, touch_event.pointer_details().tilt_x);
  EXPECT_EQ(0.0f, touch_event.pointer_details().tilt_y);

  ui::PointerDetails pointer_details(EventPointerType::kPen,
                                     /* pointer_id*/ 0,
                                     /* radius_x */ 5.0f,
                                     /* radius_y */ 6.0f,
                                     /* force */ 21.0f,
                                     /* twist */ 196,
                                     /* tilt_x */ 45.0f,
                                     /* tilt_y */ -45.0f,
                                     /* tangential_pressure */ 0.7f);
  touch_event.SetPointerDetailsForTest(pointer_details);

  EXPECT_EQ(EventPointerType::kPen, touch_event.pointer_details().pointer_type);
  EXPECT_EQ(21.0f, touch_event.pointer_details().force);
  EXPECT_EQ(45.0f, touch_event.pointer_details().tilt_x);
  EXPECT_EQ(-45.0f, touch_event.pointer_details().tilt_y);
  EXPECT_EQ(5.0f, touch_event.pointer_details().radius_x);
  EXPECT_EQ(6.0f, touch_event.pointer_details().radius_y);
  EXPECT_EQ(0.7f, touch_event.pointer_details().tangential_pressure);
  EXPECT_EQ(196, touch_event.pointer_details().twist);

  ui::TouchEvent touch_event_copy(touch_event);
  EXPECT_EQ(touch_event.pointer_details(), touch_event_copy.pointer_details());
}

TEST(EventTest, MouseEventLatencyUIComponentExists) {
  const gfx::Point origin(0, 0);
  MouseEvent mouseev(EventType::kMousePressed, origin, origin,
                     EventTimeForNow(), 0, 0);
  EXPECT_TRUE(mouseev.latency()->FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, nullptr));
}

TEST(EventTest, MouseWheelEventLatencyUIComponentExists) {
  const gfx::Point origin(0, 0);
  MouseWheelEvent mouseWheelev(gfx::Vector2d(), origin, origin,
                               EventTimeForNow(), 0, 0);
  EXPECT_TRUE(mouseWheelev.latency()->FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, nullptr));
}

TEST(EventTest, MouseWheelEventLinearTickCalculation) {
  const gfx::Point origin;
  MouseWheelEvent mouse_wheel_ev(
      gfx::Vector2d(-2 * MouseWheelEvent::kWheelDelta,
                    MouseWheelEvent::kWheelDelta),
      origin, origin, EventTimeForNow(), 0, 0);
  EXPECT_EQ(mouse_wheel_ev.tick_120ths().x(), -240);
  EXPECT_EQ(mouse_wheel_ev.tick_120ths().y(), 120);
}

TEST(EventTest, OrdinalMotionConversion) {
  const gfx::Point origin(0, 0);
  const gfx::Vector2dF movement(2.67, 3.14);

  // Model conversion depends on the class having a specific static method.
  struct OrdinalMotionConversionModel {
    static void ConvertPointToTarget(const OrdinalMotionConversionModel*,
                                     const OrdinalMotionConversionModel*,
                                     gfx::Point*) {
      // Do nothing.
    }
  } src, dst;

  MouseEvent mouseev1(EventType::kMousePressed, origin, origin,
                      EventTimeForNow(), 0, 0);
  MouseEvent::DispatcherApi(&mouseev1).set_movement(movement);
  EXPECT_EQ(mouseev1.movement(), movement);
  EXPECT_TRUE(mouseev1.flags() & EF_UNADJUSTED_MOUSE);

  MouseEvent mouseev2(mouseev1, &src, &dst);
  EXPECT_EQ(mouseev2.movement(), movement);
  EXPECT_TRUE(mouseev2.flags() & EF_UNADJUSTED_MOUSE);

  // Setting the flags in construction should override the model's.
  MouseEvent mouseev3(mouseev1, &src, &dst, EventType::kMouseMoved,
                      /* flags */ 0);
  EXPECT_EQ(mouseev3.movement(), movement);
  EXPECT_FALSE(mouseev3.flags() & EF_UNADJUSTED_MOUSE);
}

// Checks that Event.Latency.OS2.MOUSE_WHEEL histogram is computed properly.
TEST(EventTest, EventLatencyOSMouseWheelHistogram) {
#if BUILDFLAG(IS_WIN)
  base::HistogramTester histogram_tester;
  CHROME_MSG event = {nullptr, WM_MOUSEWHEEL, 0, 0};
  MouseWheelEvent mouseWheelEvent(event);
  histogram_tester.ExpectTotalCount("Event.Latency.OS2.MOUSE_WHEEL", 1);
#endif
}

TEST(EventTest, UpdateForRootTransformation) {
  gfx::Transform identity_transform;
  const gfx::Point location(10, 10);
  const gfx::Point root_location(20, 20);
  const gfx::PointF f_location(10, 10);
  const gfx::PointF f_root_location(20, 20);

  // A mouse event that is untargeted should reset the root location when
  // transformed. Though the events start out with different locations and
  // root_locations, they should be equal afterwards.
  ui::MouseEvent untargeted(EventType::kMousePressed, location, root_location,
                            EventTimeForNow(), 0, 0);
  untargeted.UpdateForRootTransform(identity_transform, identity_transform);
  EXPECT_EQ(location, untargeted.location());
  EXPECT_EQ(location, untargeted.root_location());

  ui::test::TestEventTarget target;

  // A touch event should behave the same way as others.
  {
    PointerDetails pointer_details(EventPointerType::kTouch, 0 /* pointer id */,
                                   3, 4, 50, 0 /* twist */, 0, 0);
    ui::TouchEvent targeted(EventType::kTouchPressed, f_location,
                            f_root_location, EventTimeForNow(),
                            pointer_details);
    targeted.UpdateForRootTransform(identity_transform, identity_transform);
    EXPECT_EQ(location, targeted.location());
    EXPECT_EQ(location, targeted.root_location());
    EXPECT_EQ(pointer_details, targeted.pointer_details());
  }

  // A touch event should scale the same way as others.
  {
    // Targeted event with 2x and 3x scales.
    gfx::Transform transform2x;
    transform2x.Scale(2, 2);
    gfx::Transform transform3x;
    transform3x.Scale(3, 3);
    PointerDetails pointer_details(EventPointerType::kTouch, 0 /* pointer id */,
                                   3, 4, 50, 0 /* twist */, 17.2 /* tilt_x */,
                                   -28.3 /* tilt_y */);

    ui::TouchEvent targeted(EventType::kTouchPressed, f_location,
                            f_root_location, EventTimeForNow(),
                            pointer_details);
    targeted.UpdateForRootTransform(transform2x, transform3x);
    auto updated_location = ScalePoint(f_location, 2.0f);
    EXPECT_EQ(updated_location, targeted.location_f());
    EXPECT_EQ(updated_location, targeted.root_location_f());
    auto updated_pointer_details(pointer_details);
    updated_pointer_details.radius_x *= 2;
    updated_pointer_details.radius_y *= 2;
    EXPECT_EQ(updated_pointer_details, targeted.pointer_details())
        << " orig: " << pointer_details.ToString() << " vs "
        << targeted.pointer_details().ToString();
  }

  // A touch event should rotate appropriately.
  {
    // Rotate by 90 degrees, then scale by a half or 0.75 (depending on axis),
    // and then offset by 720/1080. Note that the offset should have no impact
    // on vectors, i.e. radius.
    // The scale happens after rotation, so x should be 0.75 * the y.
    gfx::Transform rotate90;
    rotate90.Rotate(90.0f);
    rotate90.Translate(gfx::Vector2dF(720.0f, 1080.0f));
    rotate90.Scale(0.5, 0.75);
    gfx::Transform transform3x;
    transform3x.Scale(3, 3);
    PointerDetails pointer_details(EventPointerType::kTouch, 0 /* pointer id */,
                                   3, 4, 50, 0 /* twist */, -17.4 /* tilt_x */,
                                   31.2 /* tilt_y */);

    ui::TouchEvent targeted(EventType::kTouchPressed, f_location,
                            f_root_location, EventTimeForNow(),
                            pointer_details);
    Event::DispatcherApi(&targeted).set_target(&target);
    targeted.UpdateForRootTransform(rotate90, transform3x);
    auto updated_pointer_details(pointer_details);
    updated_pointer_details.radius_x = pointer_details.radius_y * 0.75;
    updated_pointer_details.radius_y = pointer_details.radius_x * 0.5;
    updated_pointer_details.tilt_x = -31.2;
    updated_pointer_details.tilt_y = -17.4;

    EXPECT_EQ(updated_pointer_details, targeted.pointer_details())
        << " orig: " << updated_pointer_details.ToString() << " vs "
        << targeted.pointer_details().ToString();
  }

  // A mouse event that is targeted should not set the root location to the
  // local location. They start with different locations and should stay
  // unequal after a transform is applied.
  {
    ui::MouseEvent targeted(EventType::kMousePressed, location, root_location,
                            EventTimeForNow(), 0, 0);
    Event::DispatcherApi(&targeted).set_target(&target);
    targeted.UpdateForRootTransform(identity_transform, identity_transform);
    EXPECT_EQ(location, targeted.location());
    EXPECT_EQ(root_location, targeted.root_location());
  }

  {
    // Targeted event with 2x and 3x scales.
    gfx::Transform transform2x;
    transform2x.Scale(2, 2);
    gfx::Transform transform3x;
    transform3x.Scale(3, 3);
    ui::MouseEvent targeted(EventType::kMousePressed, location, root_location,
                            EventTimeForNow(), 0, 0);
    Event::DispatcherApi(&targeted).set_target(&target);
    targeted.UpdateForRootTransform(transform2x, transform3x);
    EXPECT_EQ(gfx::Point(30, 30), targeted.location());
    EXPECT_EQ(gfx::Point(40, 40), targeted.root_location());
  }
}

TEST(EventTest, OperatorEqual) {
  MouseEvent m1(EventType::kMousePressed, gfx::Point(1, 2), gfx::Point(2, 3),
                EventTimeForNow(), EF_LEFT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON);
  base::flat_map<std::string, std::vector<uint8_t>> properties;
  properties["a"] = {1u};
  m1.SetProperties(properties);
  EXPECT_EQ(properties, *(m1.properties()));
  MouseEvent m2(EventType::kMouseReleased, gfx::Point(11, 21), gfx::Point(2, 2),
                EventTimeForNow(), EF_RIGHT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON);
  m2 = m1;
  ASSERT_TRUE(m2.properties());
  EXPECT_EQ(properties, *(m2.properties()));
}

// Verifies that ToString() generates something and doesn't crash. The specific
// format isn't important.
TEST(EventTest, ToStringNotEmpty) {
  MouseEvent mouse_event(EventType::kMousePressed, gfx::Point(1, 2),
                         gfx::Point(2, 3), EventTimeForNow(),
                         EF_LEFT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON);
  EXPECT_FALSE(mouse_event.ToString().empty());

  ScrollEvent scroll_event(EventType::kScroll, gfx::Point(1, 2),
                           EventTimeForNow(), EF_NONE, 1.f, 2.f, 3.f, 4.f, 1);
  EXPECT_FALSE(scroll_event.ToString().empty());
}

#if BUILDFLAG(IS_WIN)
namespace {

const struct AltGraphEventTestCase {
  KeyboardCode key_code;
  KeyboardLayout layout;
  std::vector<KeyboardCode> modifier_key_codes;
  int expected_flags;
} kAltGraphEventTestCases[] = {
    // US English -> AltRight never behaves as AltGraph.
    {VKEY_C,
     KEYBOARD_LAYOUT_ENGLISH_US,
     {VKEY_RMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALT_DOWN | EF_CONTROL_DOWN},
    {VKEY_E,
     KEYBOARD_LAYOUT_ENGLISH_US,
     {VKEY_RMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALT_DOWN | EF_CONTROL_DOWN},

    // French -> Always expect AltGraph if VKEY_RMENU is pressed.
    {VKEY_C,
     KEYBOARD_LAYOUT_FRENCH,
     {VKEY_RMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALTGR_DOWN},
    {VKEY_E,
     KEYBOARD_LAYOUT_FRENCH,
     {VKEY_RMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALTGR_DOWN},

    // French -> Expect Control+Alt is AltGraph on AltGraph-shifted keys.
    {VKEY_C,
     KEYBOARD_LAYOUT_FRENCH,
     {VKEY_LMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALT_DOWN | EF_CONTROL_DOWN},
    {VKEY_E,
     KEYBOARD_LAYOUT_FRENCH,
     {VKEY_LMENU, VKEY_LCONTROL, VKEY_MENU, VKEY_CONTROL},
     EF_ALTGR_DOWN},
};

class AltGraphEventTest
    : public testing::TestWithParam<std::tuple<UINT, AltGraphEventTestCase>> {
 public:
  AltGraphEventTest()
      : msg_({nullptr, message_type(),
              static_cast<WPARAM>(test_case().key_code)}) {
    // Save the current keyboard layout and state, to restore later.
    CHECK(GetKeyboardState(original_keyboard_state_));
    original_keyboard_layout_ = GetKeyboardLayout(0);

    // Configure specified layout, and update keyboard state for specified
    // modifier keys.
    CHECK(ActivateKeyboardLayout(GetPlatformKeyboardLayout(test_case().layout),
                                 0));
    BYTE test_keyboard_state[256] = {};
    for (const auto& key_code : test_case().modifier_key_codes)
      test_keyboard_state[key_code] = 0x80;
    CHECK(SetKeyboardState(test_keyboard_state));
  }

  ~AltGraphEventTest() {
    // Restore the original keyboard layout & key states.
    CHECK(ActivateKeyboardLayout(original_keyboard_layout_, 0));
    CHECK(SetKeyboardState(original_keyboard_state_));
  }

 protected:
  UINT message_type() const { return std::get<0>(GetParam()); }
  const AltGraphEventTestCase& test_case() const {
    return std::get<1>(GetParam());
  }

  const CHROME_MSG msg_;
  BYTE original_keyboard_state_[256] = {};
  HKL original_keyboard_layout_ = nullptr;
};

}  // namespace

TEST_P(AltGraphEventTest, KeyEventAltGraphModifer) {
  KeyEvent event(msg_);
  if (message_type() == WM_CHAR) {
    // By definition, if we receive a WM_CHAR message when Control and Alt are
    // pressed, it indicates AltGraph.
    EXPECT_EQ(event.flags() & (EF_CONTROL_DOWN | EF_ALT_DOWN | EF_ALTGR_DOWN),
              EF_ALTGR_DOWN);
  } else {
    EXPECT_EQ(event.flags() & (EF_CONTROL_DOWN | EF_ALT_DOWN | EF_ALTGR_DOWN),
              test_case().expected_flags);
  }
}

INSTANTIATE_TEST_SUITE_P(
    WM_KEY,
    AltGraphEventTest,
    ::testing::Combine(::testing::Values(WM_KEYDOWN, WM_KEYUP),
                       ::testing::ValuesIn(kAltGraphEventTestCases)));
INSTANTIATE_TEST_SUITE_P(
    WM_CHAR,
    AltGraphEventTest,
    ::testing::Combine(::testing::Values(WM_CHAR),
                       ::testing::ValuesIn(kAltGraphEventTestCases)));

// Tests for ComputeEventLatencyOS variants.

class EventLatencyTest : public ::testing::Test {
 public:
  EventLatencyTest() { SetEventLatencyTickClockForTesting(&tick_clock_); }

  ~EventLatencyTest() override { SetEventLatencyTickClockForTesting(nullptr); }

 protected:
  void UpdateTickClock(DWORD timestamp) {
    tick_clock_.SetNowTicks(base::TimeTicks() + base::Milliseconds(timestamp));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // |task_environment_| mocks the base::TimeTicks clock while |tick_clock_|
  // mocks ::GetTickCount.
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(EventLatencyTest, ComputeEventLatencyOSFromTickCount) {
  // Mock a tick clock at 16ms (it's 15.625ms and alternates between 15 and 16ms
  // in practice but that's irrelevant for this mock).
  constexpr base::TimeDelta kTickInterval = base::Milliseconds(16);

  // Create events whose timestamps are 5 ticks away from looping around the max
  // range of ::GetTickCount.
  constexpr DWORD timestamp_msec =
      std::numeric_limits<DWORD>::max() -
      // Remove any portion that's not kTickInterval aligned.
      (std::numeric_limits<DWORD>::max() % kTickInterval.InMilliseconds()) -
      4 * kTickInterval.InMilliseconds();
  constexpr TOUCHINPUT touch_input = {
      .dwTime = timestamp_msec,
  };
  constexpr POINTER_INFO pointer_info = {
      .dwTime = timestamp_msec,
      .PerformanceCount = 0UL,
  };

  // This test will create several events with the same timestamp, and change
  // the mocked result of ::GetTickCount for each measurement. This makes it
  // easier to test the edge case when the 32-bit ::GetTickCount overflows.

  // Expect 0 within the same tick.
  UpdateTickClock(timestamp_msec);
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Milliseconds(0), 2);
  }

  // Expect 0 within the next tick (optimistically assume the event could have
  // been generated at the very end of the last tick).
  UpdateTickClock(timestamp_msec + kTickInterval.InMilliseconds());
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Milliseconds(0), 2);
  }

  // Expect 16ms within two ticks (again, optimistic for the first tick
  // interval).
  UpdateTickClock(timestamp_msec + 2 * kTickInterval.InMilliseconds());
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Milliseconds(16), 2);
  }

  // Expect 16ms within two ticks even if both ticked at the lower-end of the
  // 64hZ clock (15ms).
  constexpr DWORD kTickIntervalLowEnd = base::Hertz(64).InMilliseconds();
  static_assert(kTickIntervalLowEnd == 15);
  UpdateTickClock(timestamp_msec + 2 * kTickIntervalLowEnd);
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Milliseconds(16), 2);
  }

  // Simulate ::GetTickCount wrapping around (expecting 4 * kTickInterval
  // reported as 1 * kTickInterval is discounted).
  constexpr DWORD wrapped_timestamp_msec =
      timestamp_msec + 5 * static_cast<DWORD>(kTickInterval.InMilliseconds());
  static_assert(wrapped_timestamp_msec == 0,
                "timestamp should have wrapped around");
  UpdateTickClock(wrapped_timestamp_msec);
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            4 * kTickInterval, 2);
  }

  // Simulate ::GetTickCount wrapping around multiple intervals. Conveniently,
  // 15 intervals yields an expected optimistic 14 intervals which is 224ms and
  // lands precisely on the boundary of the logarithmic timing histogram being
  // used, catching off-by-one errors (which a previous implementation had when
  // it reported 223ms in this test).
  constexpr DWORD wrapped_timestamp_msec2 =
      timestamp_msec + 15 * static_cast<DWORD>(kTickInterval.InMilliseconds());
  static_assert(wrapped_timestamp_msec2 == 10 * kTickInterval.InMilliseconds(),
                "timestamp should have wrapped around");
  UpdateTickClock(wrapped_timestamp_msec2);
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            14 * kTickInterval, 2);
  }

  // Expect 0 if the clock is somehow reported as behind the event time.
  UpdateTickClock(timestamp_msec - kTickInterval.InMilliseconds());
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::TimeDelta(), 2);
  }

  // Expect 0 if the clock is reported as too far ahead (protection against
  // bogus event time stamps).
  UpdateTickClock(timestamp_msec + 300 * 1000);
  {
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromTOUCHINPUT(EventType::kTouchPressed, touch_input,
                                        base::TimeTicks::Now());
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::TimeDelta(), 2);
  }
}

TEST_F(EventLatencyTest, ComputeEventLatencyOSFromPerformanceCounter) {
  // Make sure there's enough time before Now() to create an event that's
  // several minutes old.
  task_environment_.AdvanceClock(base::Minutes(5));

  // Convert the current time to units directly compatible with the Performance
  // Counter.
  LARGE_INTEGER ticks_per_sec = {};
  if (!::QueryPerformanceFrequency(&ticks_per_sec) ||
      ticks_per_sec.QuadPart <= 0 || !base::TimeTicks::IsHighResolution()) {
    // Skip this test when the performance counter is unavailable or
    // unreliable. (It's unlikely, but possible, that IsHighResolution is false
    // even if the performance counter works - see InitializeNowFunctionPointer
    // in time_win.cc - so also skip the test in this case.)
    return;
  }
  const auto ticks_per_second = ticks_per_sec.QuadPart;
  UINT64 current_timestamp =
      base::TimeTicks::Now().since_origin().InSecondsF() * ticks_per_second;

  // Event created shortly before now.
  {
    const POINTER_INFO pointer_info = {
        .dwTime = 0U,
        .PerformanceCount = current_timestamp - ticks_per_second,
    };
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Seconds(1), 1);
  }

  // Event created several minutes before now (IsValidTimebase should return
  // false). The delta should be recorded as 0.
  {
    const POINTER_INFO pointer_info = {
        .dwTime = 0U,
        .PerformanceCount = current_timestamp - 5 * 60 * ticks_per_second,
    };
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::TimeDelta(), 1);
  }

  // Event created in the future (IsValidTimebase should return false). The
  // delta should be recorded as 0.
  {
    const POINTER_INFO pointer_info = {
        .dwTime = 0U,
        .PerformanceCount = current_timestamp + ticks_per_second,
    };
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::TimeDelta(), 1);
  }

  // Invalid event with no timestamp.
  {
    const POINTER_INFO pointer_info = {
        .dwTime = 0U,
        .PerformanceCount = 0UL,
    };
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectTotalCount("Event.Latency.OS2.TOUCH_PRESSED", 0);
  }

  // Invalid event with 2 timestamps should take the higher-precision one.
  {
    const DWORD now_msec = 1000;
    UpdateTickClock(now_msec);

    const POINTER_INFO pointer_info = {
        // 10 milliseconds ago.
        .dwTime = now_msec - 10,
        // 1 second ago.
        .PerformanceCount = current_timestamp - ticks_per_second,
    };
    base::HistogramTester histogram_tester;
    ComputeEventLatencyOSFromPOINTER_INFO(EventType::kTouchPressed,
                                          pointer_info, base::TimeTicks::Now());
    histogram_tester.ExpectUniqueTimeSample("Event.Latency.OS2.TOUCH_PRESSED",
                                            base::Seconds(1), 1);
  }
}

#endif  // BUILDFLAG(IS_WIN)

// Verifies that copied events never copy target_.
TEST(EventTest, NeverCopyTarget) {
  const gfx::Point location(10, 10);
  const gfx::Point root_location(20, 20);
  ui::test::TestEventTarget target;

  ui::MouseEvent targeted(EventType::kMousePressed, location, root_location,
                          EventTimeForNow(), 0, 0);
  Event::DispatcherApi(&targeted).set_target(&target);
  ui::MouseEvent targeted_copy1(targeted);

  EXPECT_EQ(nullptr, targeted_copy1.target());

  ui::MouseEvent targeted_copy2 = targeted;

  EXPECT_EQ(nullptr, targeted_copy2.target());
}

// Verify if a character event is created.
TEST(EventTest, CreateCharcterEvent) {
  KeyEvent key_event =
      ui::KeyEvent::FromCharacter(0x5A, VKEY_Z, ui::DomCode::NONE, EF_NONE);
  EXPECT_TRUE(key_event.is_char());
  EXPECT_EQ(0x5A, key_event.GetCharacter());
  EXPECT_EQ(VKEY_Z, key_event.key_code());
  EXPECT_EQ(EF_NONE, key_event.flags());
}

}  // namespace ui
