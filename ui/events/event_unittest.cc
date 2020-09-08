// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/test/test_event_target.h"
#include "ui/gfx/transform.h"

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/x11_event_translation.h"  // nogncheck
#include "ui/gfx/x/event.h"                     // nogncheck
#include "ui/gfx/x/x11.h"                       // nogncheck
#include "ui/gfx/x/x11_types.h"                 // nogncheck
#endif

namespace ui {

TEST(EventTest, NoNativeEvent) {
  KeyEvent keyev(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE);
  EXPECT_FALSE(keyev.HasNativeEvent());
}

TEST(EventTest, NativeEvent) {
#if defined(OS_WIN)
  MSG native_event = {nullptr, WM_KEYUP, VKEY_A, 0};
  KeyEvent keyev(native_event);
  EXPECT_TRUE(keyev.HasNativeEvent());
#endif
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    ScopedXI2Event event;
    event.InitKeyEvent(ET_KEY_RELEASED, VKEY_A, EF_NONE);
    auto keyev = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_FALSE(keyev->HasNativeEvent());
  }
#endif
}

TEST(EventTest, GetCharacter) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  // Check if Control+Enter returns 10.
  KeyEvent keyev1(ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN);
  EXPECT_EQ(10, keyev1.GetCharacter());
  // Check if Enter returns 13.
  KeyEvent keyev2(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);
  EXPECT_EQ(13, keyev2.GetCharacter());

#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    // For X11, test the functions with native_event() as well. crbug.com/107837
    ScopedXI2Event event;
    event.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN);
    auto keyev3 = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(10, keyev3->GetCharacter());

    event.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);
    auto keyev4 = ui::BuildKeyEventFromXEvent(*event);
    EXPECT_EQ(13, keyev4->GetCharacter());
  }
#endif

  // Check if expected Unicode character was returned for a key combination
  // contains Control.
  // e.g. Control+Shift+2 produces U+200C on "Persian" keyboard.
  // http://crbug.com/582453
  KeyEvent keyev5(0x200C, VKEY_UNKNOWN, ui::DomCode::NONE,
                  EF_CONTROL_DOWN | EF_SHIFT_DOWN);
  EXPECT_EQ(0x200C, keyev5.GetCharacter());
}

TEST(EventTest, ClickCount) {
  const gfx::Point origin(0, 0);
  MouseEvent mouseev(ET_MOUSE_PRESSED, origin, origin, EventTimeForNow(), 0, 0);
  for (int i = 1; i <= 3; ++i) {
    mouseev.SetClickCount(i);
    EXPECT_EQ(i, mouseev.GetClickCount());
  }
}

TEST(EventTest, RepeatedClick) {
  const gfx::Point origin(0, 0);
  MouseEvent event1(ET_MOUSE_PRESSED, origin, origin, EventTimeForNow(), 0, 0);
  MouseEvent event2(ET_MOUSE_PRESSED, origin, origin, EventTimeForNow(), 0, 0);
  LocatedEventTestApi test_event1(&event1);
  LocatedEventTestApi test_event2(&event2);

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks soon = start + base::TimeDelta::FromMilliseconds(1);
  base::TimeTicks later = start + base::TimeDelta::FromMilliseconds(1000);

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

TEST(EventTest, RepeatedKeyEvent) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks time1 = start + base::TimeDelta::FromMilliseconds(1);
  base::TimeTicks time2 = start + base::TimeDelta::FromMilliseconds(2);
  base::TimeTicks time3 = start + base::TimeDelta::FromMilliseconds(3);

  KeyEvent event1(ET_KEY_PRESSED, VKEY_A, 0, start);
  KeyEvent event2(ET_KEY_PRESSED, VKEY_A, 0, time1);
  KeyEvent event3(ET_KEY_PRESSED, VKEY_A, EF_LEFT_MOUSE_BUTTON, time2);
  KeyEvent event4(ET_KEY_PRESSED, VKEY_A, 0, time3);

  event1.InitializeNative();
  EXPECT_TRUE((event1.flags() & EF_IS_REPEAT) == 0);
  event2.InitializeNative();
  EXPECT_TRUE((event2.flags() & EF_IS_REPEAT) != 0);

  event3.InitializeNative();
  EXPECT_TRUE((event3.flags() & EF_IS_REPEAT) != 0);

  event4.InitializeNative();
  EXPECT_TRUE((event4.flags() & EF_IS_REPEAT) != 0);
}

// Tests that re-processing the same mouse press event (detected by timestamp)
// does not yield a double click event: http://crbug.com/389162
TEST(EventTest, DoubleClickRequiresUniqueTimestamp) {
  const gfx::Point point(0, 0);
  base::TimeTicks time1 = base::TimeTicks::Now();
  base::TimeTicks time2 = time1 + base::TimeDelta::FromMilliseconds(1);

  // Re-processing the same press doesn't yield a double-click.
  MouseEvent event(ET_MOUSE_PRESSED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  // Processing a press with the same timestamp doesn't yield a double-click.
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  // Processing a press with a later timestamp does yield a double-click.
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();

  // Test processing a double press and release sequence with one timestamp.
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_RELEASED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_RELEASED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();

  // Test processing a double press and release sequence with two timestamps.
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_RELEASED, point, point, time1, 0, 0);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_RELEASED, point, point, time2, 0, 0);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(event));
  MouseEvent::ResetLastClickForTest();
}

// Tests that right clicking, then left clicking does not yield double clicks.
TEST(EventTest, SingleClickRightLeft) {
  const gfx::Point point(0, 0);
  base::TimeTicks time1 = base::TimeTicks::Now();
  base::TimeTicks time2 = time1 + base::TimeDelta::FromMilliseconds(1);
  base::TimeTicks time3 = time1 + base::TimeDelta::FromMilliseconds(2);

  MouseEvent event(ET_MOUSE_PRESSED, point, point, time1,
                   ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time2,
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_RELEASED, point, point, time2,
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(event));
  event = MouseEvent(ET_MOUSE_PRESSED, point, point, time3,
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

  for (size_t i = 0; i < base::size(kTestData); ++i) {
    KeyEvent key(ET_KEY_PRESSED, kTestData[i].key_code, kTestData[i].flags);
    EXPECT_EQ(kTestData[i].character, key.GetCharacter())
        << " Index:" << i << " key_code:" << kTestData[i].key_code;
  }
}

TEST(EventTest, KeyEventDirectUnicode) {
  KeyEvent key(0x1234U, ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE);
  EXPECT_EQ(0x1234U, key.GetCharacter());
  EXPECT_EQ(ET_KEY_PRESSED, key.type());
  EXPECT_TRUE(key.is_char());
}

TEST(EventTest, NormalizeKeyEventFlags) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    // Normalize flags when KeyEvent is created from XEvent.
    ScopedXI2Event event;
    {
      event.InitKeyEvent(ET_KEY_PRESSED, VKEY_SHIFT, EF_SHIFT_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_SHIFT_DOWN, keyev->flags());
    }
    {
      event.InitKeyEvent(ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_NONE, keyev->flags());
    }
    {
      event.InitKeyEvent(ET_KEY_PRESSED, VKEY_CONTROL, EF_CONTROL_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_CONTROL_DOWN, keyev->flags());
    }
    {
      event.InitKeyEvent(ET_KEY_RELEASED, VKEY_CONTROL, EF_CONTROL_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_NONE, keyev->flags());
    }
    {
      event.InitKeyEvent(ET_KEY_PRESSED, VKEY_MENU, EF_ALT_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_ALT_DOWN, keyev->flags());
    }
    {
      event.InitKeyEvent(ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN);
      auto keyev = ui::BuildKeyEventFromXEvent(*event);
      EXPECT_EQ(EF_NONE, keyev->flags());
    }
  }
#endif

  // Do not normalize flags for synthesized events without
  // KeyEvent::NormalizeFlags called explicitly.
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_SHIFT, EF_SHIFT_DOWN);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_CONTROL, EF_CONTROL_DOWN);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_CONTROL, EF_CONTROL_DOWN);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_MENU, EF_ALT_DOWN);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
}

TEST(EventTest, KeyEventCopy) {
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, EF_NONE);
  std::unique_ptr<KeyEvent> copied_key(new KeyEvent(key));
  EXPECT_EQ(copied_key->type(), key.type());
  EXPECT_EQ(copied_key->key_code(), key.key_code());
}

TEST(EventTest, KeyEventCode) {
  const DomCode kDomCodeForSpace = DomCode::SPACE;
  const char kCodeForSpace[] = "Space";
  ASSERT_EQ(kDomCodeForSpace,
            ui::KeycodeConverter::CodeStringToDomCode(kCodeForSpace));
  const uint16_t kNativeCodeSpace =
      ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace);
  ASSERT_NE(ui::KeycodeConverter::InvalidNativeKeycode(), kNativeCodeSpace);
  ASSERT_EQ(kNativeCodeSpace,
            ui::KeycodeConverter::DomCodeToNativeKeycode(kDomCodeForSpace));

  {
    KeyEvent key(ET_KEY_PRESSED, VKEY_SPACE, kDomCodeForSpace, EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
  {
    // Regardless the KeyEvent.key_code (VKEY_RETURN), code should be
    // the specified value.
    KeyEvent key(ET_KEY_PRESSED, VKEY_RETURN, kDomCodeForSpace, EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
  {
    // If the synthetic event is initialized without code, the code is
    // determined from the KeyboardCode assuming a US keyboard layout.
    KeyEvent key(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE);
    EXPECT_EQ(kCodeForSpace, key.GetCodeString());
  }
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    // KeyEvent converts from the native keycode (XKB) to the code.
    ScopedXI2Event xevent;
    xevent.InitKeyEvent(ET_KEY_PRESSED, VKEY_SPACE, kNativeCodeSpace);
    auto keyev = ui::BuildKeyEventFromXEvent(*xevent);
    EXPECT_EQ(kCodeForSpace, keyev->GetCodeString());
  }
#endif  // USE_X11
#if defined(OS_WIN)
  {
    // Test a non extended key.
    ASSERT_EQ((kNativeCodeSpace & 0xFF), kNativeCodeSpace);

    const LPARAM lParam = GetLParamFromScanCode(kNativeCodeSpace);
    MSG native_event = {nullptr, WM_KEYUP, VKEY_SPACE, lParam};
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

    MSG native_event = {nullptr, WM_KEYUP, VKEY_HOME, lParam};
    KeyEvent key(native_event);

    // KeyEvent converts from the native keycode (scan code) to the code.
    EXPECT_EQ(kCodeForHome, key.GetCodeString());
  }
#endif  // OS_WIN
}

#if defined(USE_X11)
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

TEST(EventTest, AutoRepeat) {
  if (features::IsUsingOzonePlatform())
    return;
  const uint16_t kNativeCodeA =
      ui::KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_A);
  const uint16_t kNativeCodeB =
      ui::KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_B);

  ScopedXI2Event native_event_a_pressed;
  native_event_a_pressed.InitKeyEvent(ET_KEY_PRESSED, VKEY_A, kNativeCodeA);
  ScopedXI2Event native_event_a_pressed_1500;
  native_event_a_pressed_1500.InitKeyEvent(ET_KEY_PRESSED, VKEY_A,
                                           kNativeCodeA);
  ScopedXI2Event native_event_a_pressed_3000;
  native_event_a_pressed_3000.InitKeyEvent(ET_KEY_PRESSED, VKEY_A,
                                           kNativeCodeA);

  ScopedXI2Event native_event_a_released;
  native_event_a_released.InitKeyEvent(ET_KEY_RELEASED, VKEY_A, kNativeCodeA);
  ScopedXI2Event native_event_b_pressed;
  native_event_b_pressed.InitKeyEvent(ET_KEY_PRESSED, VKEY_B, kNativeCodeB);
  ScopedXI2Event native_event_a_pressed_nonstandard_state;
  native_event_a_pressed_nonstandard_state.InitKeyEvent(ET_KEY_PRESSED, VKEY_A,
                                                        kNativeCodeA);
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
#endif  // USE_X11

TEST(EventTest, TouchEventRadiusDefaultsToOtherAxis) {
  const base::TimeTicks time = base::TimeTicks::Now();
  const float non_zero_length1 = 30;
  const float non_zero_length2 = 46;

  TouchEvent event1(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                    PointerDetails(ui::EventPointerType::kTouch,
                                   /* pointer_id*/ 0,
                                   /* radius_x */ non_zero_length1,
                                   /* radius_y */ 0.0f,
                                   /* force */ 0));
  EXPECT_EQ(non_zero_length1, event1.pointer_details().radius_x);
  EXPECT_EQ(non_zero_length1, event1.pointer_details().radius_y);

  TouchEvent event2(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
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
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_in_range),
                     0);
    EXPECT_FLOAT_EQ(angle_in_range, event.ComputeRotationAngle());
  }

  {
    const float angle_in_range = 179.9f;
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_in_range),
                     0);
    EXPECT_FLOAT_EQ(angle_in_range, event.ComputeRotationAngle());
  }

  {
    const float angle_negative = -0.1f;
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_negative),
                     0);
    EXPECT_FLOAT_EQ(180 - 0.1f, event.ComputeRotationAngle());
  }

  {
    const float angle_negative = -200;
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_negative),
                     0);
    EXPECT_FLOAT_EQ(360 - 200, event.ComputeRotationAngle());
  }

  {
    const float angle_too_big = 180;
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_too_big),
                     0);
    EXPECT_FLOAT_EQ(0, event.ComputeRotationAngle());
  }

  {
    const float angle_too_big = 400;
    TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), time,
                     PointerDetails(ui::EventPointerType::kTouch,
                                    /* pointer_id*/ 0, radius_x, radius_y,
                                    /* force */ 0, angle_too_big),
                     0);
    EXPECT_FLOAT_EQ(400 - 360, event.ComputeRotationAngle());
  }
}

TEST(EventTest, PointerDetailsTouch) {
  ui::TouchEvent touch_event_plain(
      ET_TOUCH_PRESSED, gfx::Point(0, 0), ui::EventTimeForNow(),
      PointerDetails(ui::EventPointerType::kTouch, 0));

  EXPECT_EQ(EventPointerType::kTouch,
            touch_event_plain.pointer_details().pointer_type);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().radius_x);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().radius_y);
  EXPECT_TRUE(std::isnan(touch_event_plain.pointer_details().force));
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().tilt_x);
  EXPECT_EQ(0.0f, touch_event_plain.pointer_details().tilt_y);

  ui::TouchEvent touch_event_with_details(
      ET_TOUCH_PRESSED, gfx::Point(0, 0), ui::EventTimeForNow(),
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
  ui::MouseEvent mouse_event(ET_MOUSE_PRESSED, gfx::Point(0, 0),
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

  ui::MouseEvent stylus_event(ET_MOUSE_PRESSED, gfx::Point(0, 0),
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
  ui::TouchEvent touch_event(ET_TOUCH_PRESSED, gfx::Point(0, 0),
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
  MouseEvent mouseev(ET_MOUSE_PRESSED, origin, origin, EventTimeForNow(), 0, 0);
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

  MouseEvent mouseev1(ET_MOUSE_PRESSED, origin, origin, EventTimeForNow(), 0,
                      0);
  MouseEvent::DispatcherApi(&mouseev1).set_movement(movement);
  EXPECT_EQ(mouseev1.movement(), movement);
  EXPECT_TRUE(mouseev1.flags() & EF_UNADJUSTED_MOUSE);

  MouseEvent mouseev2(mouseev1, &src, &dst);
  EXPECT_EQ(mouseev2.movement(), movement);
  EXPECT_TRUE(mouseev2.flags() & EF_UNADJUSTED_MOUSE);

  // Setting the flags in construction should override the model's.
  MouseEvent mouseev3(mouseev1, &src, &dst, EventType::ET_MOUSE_MOVED,
                      /* flags */ 0);
  EXPECT_EQ(mouseev3.movement(), movement);
  EXPECT_FALSE(mouseev3.flags() & EF_UNADJUSTED_MOUSE);
}

// Checks that Event.Latency.OS.TOUCH_PRESSED, TOUCH_MOVED,
// and TOUCH_RELEASED histograms are computed properly.
#if defined(USE_X11)
TEST(EventTest, EventLatencyOSTouchHistograms) {
  if (features::IsUsingOzonePlatform())
    return;
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
  histogram_tester.ExpectTotalCount("Event.Latency.OS.TOUCH_PRESSED", 1);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchUpdate, 5,
                               gfx::Point(20, 20), {});
  auto touch_update = ui::BuildTouchEventFromXEvent(*scoped_xevent);
  histogram_tester.ExpectTotalCount("Event.Latency.OS.TOUCH_MOVED", 1);
  scoped_xevent.InitTouchEvent(0, x11::Input::DeviceEvent::TouchEnd, 5,
                               gfx::Point(30, 30), {});
  auto touch_end = ui::BuildTouchEventFromXEvent(*scoped_xevent);
  histogram_tester.ExpectTotalCount("Event.Latency.OS.TOUCH_RELEASED", 1);
}
#endif

// Checks that Event.Latency.OS.MOUSE_WHEEL histogram is computed properly.
TEST(EventTest, EventLatencyOSMouseWheelHistogram) {
#if defined(OS_WIN)
  base::HistogramTester histogram_tester;
  MSG event = {nullptr, WM_MOUSEWHEEL, 0, 0};
  MouseWheelEvent mouseWheelEvent(event);
  histogram_tester.ExpectTotalCount("Event.Latency.OS.MOUSE_WHEEL", 1);
#endif
#if defined(USE_X11)
  if (features::IsUsingOzonePlatform())
    return;
  base::HistogramTester histogram_tester;
  DeviceDataManagerX11::CreateInstance();

  // Initializes a native event and uses it to generate a MouseWheel event.
  xcb_generic_event_t ge;
  memset(&ge, 0, sizeof(ge));
  auto* button = reinterpret_cast<xcb_button_press_event_t*>(&ge);
  button->response_type = x11::ButtonEvent::Press;
  button->detail = 4;  // A valid wheel button number between min and max.
  x11::Event native_event(&ge, x11::Connection::Get());
  auto mouse_ev = ui::BuildMouseWheelEventFromXEvent(native_event);
  histogram_tester.ExpectTotalCount("Event.Latency.OS.MOUSE_WHEEL", 1);
#endif
}

TEST(EventTest, UpdateForRootTransformation) {
  gfx::Transform identity_transform;
  const gfx::Point location(10, 10);
  const gfx::Point root_location(20, 20);

  // A mouse event that is untargeted should reset the root location when
  // transformed. Though the events start out with different locations and
  // root_locations, they should be equal afterwards.
  ui::MouseEvent untargeted(ET_MOUSE_PRESSED, location, root_location,
                            EventTimeForNow(), 0, 0);
  untargeted.UpdateForRootTransform(identity_transform, identity_transform);
  EXPECT_EQ(location, untargeted.location());
  EXPECT_EQ(location, untargeted.root_location());

  ui::test::TestEventTarget target;

  // A mouse event that is targeted should not set the root location to the
  // local location. They start with different locations and should stay
  // unequal after a transform is applied.
  {
    ui::MouseEvent targeted(ET_MOUSE_PRESSED, location, root_location,
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
    ui::MouseEvent targeted(ET_MOUSE_PRESSED, location, root_location,
                            EventTimeForNow(), 0, 0);
    Event::DispatcherApi(&targeted).set_target(&target);
    targeted.UpdateForRootTransform(transform2x, transform3x);
    EXPECT_EQ(gfx::Point(30, 30), targeted.location());
    EXPECT_EQ(gfx::Point(40, 40), targeted.root_location());
  }
}

TEST(EventTest, OperatorEqual) {
  MouseEvent m1(ET_MOUSE_PRESSED, gfx::Point(1, 2), gfx::Point(2, 3),
                EventTimeForNow(), EF_LEFT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON);
  base::flat_map<std::string, std::vector<uint8_t>> properties;
  properties["a"] = {1u};
  m1.SetProperties(properties);
  EXPECT_EQ(properties, *(m1.properties()));
  MouseEvent m2(ET_MOUSE_RELEASED, gfx::Point(11, 21), gfx::Point(2, 2),
                EventTimeForNow(), EF_RIGHT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON);
  m2 = m1;
  ASSERT_TRUE(m2.properties());
  EXPECT_EQ(properties, *(m2.properties()));
}

// Verifies that ToString() generates something and doesn't crash. The specific
// format isn't important.
TEST(EventTest, ToStringNotEmpty) {
  MouseEvent mouse_event(ET_MOUSE_PRESSED, gfx::Point(1, 2), gfx::Point(2, 3),
                         EventTimeForNow(), EF_LEFT_MOUSE_BUTTON,
                         EF_RIGHT_MOUSE_BUTTON);
  EXPECT_FALSE(mouse_event.ToString().empty());

  ScrollEvent scroll_event(ET_SCROLL, gfx::Point(1, 2), EventTimeForNow(),
                           EF_NONE, 1.f, 2.f, 3.f, 4.f, 1);
  EXPECT_FALSE(scroll_event.ToString().empty());
}

#if defined(OS_WIN)
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

  const MSG msg_;
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

#endif  // defined(OS_WIN)

}  // namespace ui
