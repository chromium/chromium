// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/blink/web_input_event.h"

#include <cstddef>
#include <cstdint>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/keyboard_layout.h"

namespace ui {

// Checks that MakeWebKeyboardEvent makes a DOM3 spec compliant key event.
// crbug.com/127142
TEST(WebInputEventTest, TestMakeWebKeyboardEvent) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  {
    // Press left Ctrl.
    KeyEvent event(EventType::kKeyPressed, VKEY_CONTROL, DomCode::CONTROL_LEFT,
                   EF_CONTROL_DOWN);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(blink::WebInputEvent::kControlKey | blink::WebInputEvent::kIsLeft,
              webkit_event.GetModifiers());
    EXPECT_EQ(static_cast<int>(DomCode::CONTROL_LEFT), webkit_event.dom_code);
    EXPECT_EQ(DomKey::CONTROL, webkit_event.dom_key);
  }
  {
    // Release left Ctrl.
    KeyEvent event(EventType::kKeyReleased, VKEY_CONTROL, DomCode::CONTROL_LEFT,
                   EF_NONE);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(blink::WebInputEvent::kIsLeft, webkit_event.GetModifiers());
    EXPECT_EQ(static_cast<int>(DomCode::CONTROL_LEFT), webkit_event.dom_code);
    EXPECT_EQ(DomKey::CONTROL, webkit_event.dom_key);
  }
  {
    // Press right Ctrl.
    KeyEvent event(EventType::kKeyPressed, VKEY_CONTROL, DomCode::CONTROL_RIGHT,
                   EF_CONTROL_DOWN);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(
        blink::WebInputEvent::kControlKey | blink::WebInputEvent::kIsRight,
        webkit_event.GetModifiers());
    EXPECT_EQ(static_cast<int>(DomCode::CONTROL_RIGHT), webkit_event.dom_code);
  }
  {
    // Release right Ctrl.
    KeyEvent event(EventType::kKeyReleased, VKEY_CONTROL,
                   DomCode::CONTROL_RIGHT, EF_NONE);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(blink::WebInputEvent::kIsRight, webkit_event.GetModifiers());
    EXPECT_EQ(static_cast<int>(DomCode::CONTROL_RIGHT), webkit_event.dom_code);
    EXPECT_EQ(DomKey::CONTROL, webkit_event.dom_key);
  }
}

TEST(WebInputEventTest, TestMakeWebKeyboardEventWindowsKeyCode) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  {
    // Press left Ctrl.
    KeyEvent event(EventType::kKeyPressed, VKEY_CONTROL, DomCode::CONTROL_LEFT,
                   EF_CONTROL_DOWN, DomKey::CONTROL, EventTimeForNow());
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    EXPECT_EQ(VKEY_CONTROL, webkit_event.windows_key_code);
  }
  {
    // Press right Ctrl.
    KeyEvent event(EventType::kKeyPressed, VKEY_CONTROL, DomCode::CONTROL_RIGHT,
                   EF_CONTROL_DOWN, DomKey::CONTROL, EventTimeForNow());
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    EXPECT_EQ(VKEY_CONTROL, webkit_event.windows_key_code);
  }
#if BUILDFLAG(IS_WIN)
// TODO(yusukes): Add tests for win_aura once keyboardEvent() in
// third_party/WebKit/Source/web/win/WebInputEventFactory.cpp is modified
// to return VKEY_[LR]XXX instead of VKEY_XXX.
// https://bugs.webkit.org/show_bug.cgi?id=86694
#endif
}

// Checks that MakeWebKeyboardEvent fills a correct keypad modifier.
TEST(WebInputEventTest, TestMakeWebKeyboardEventKeyPadKeyCode) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  struct TestCase {
    DomCode dom_code;         // The physical key (location).
    KeyboardCode ui_keycode;  // The virtual key code.
    bool expected_result;     // true if the event has "isKeyPad" modifier.
  } kTesCases[] = {
      {DomCode::DIGIT0, VKEY_0, false},
      {DomCode::DIGIT1, VKEY_1, false},
      {DomCode::DIGIT2, VKEY_2, false},
      {DomCode::DIGIT3, VKEY_3, false},
      {DomCode::DIGIT4, VKEY_4, false},
      {DomCode::DIGIT5, VKEY_5, false},
      {DomCode::DIGIT6, VKEY_6, false},
      {DomCode::DIGIT7, VKEY_7, false},
      {DomCode::DIGIT8, VKEY_8, false},
      {DomCode::DIGIT9, VKEY_9, false},

      {DomCode::NUMPAD0, VKEY_NUMPAD0, true},
      {DomCode::NUMPAD1, VKEY_NUMPAD1, true},
      {DomCode::NUMPAD2, VKEY_NUMPAD2, true},
      {DomCode::NUMPAD3, VKEY_NUMPAD3, true},
      {DomCode::NUMPAD4, VKEY_NUMPAD4, true},
      {DomCode::NUMPAD5, VKEY_NUMPAD5, true},
      {DomCode::NUMPAD6, VKEY_NUMPAD6, true},
      {DomCode::NUMPAD7, VKEY_NUMPAD7, true},
      {DomCode::NUMPAD8, VKEY_NUMPAD8, true},
      {DomCode::NUMPAD9, VKEY_NUMPAD9, true},

      {DomCode::NUMPAD_MULTIPLY, VKEY_MULTIPLY, true},
      {DomCode::NUMPAD_SUBTRACT, VKEY_SUBTRACT, true},
      {DomCode::NUMPAD_ADD, VKEY_ADD, true},
      {DomCode::NUMPAD_DIVIDE, VKEY_DIVIDE, true},
      {DomCode::NUMPAD_DECIMAL, VKEY_DECIMAL, true},
      {DomCode::NUMPAD_DECIMAL, VKEY_DELETE, true},
      {DomCode::NUMPAD0, VKEY_INSERT, true},
      {DomCode::NUMPAD1, VKEY_END, true},
      {DomCode::NUMPAD2, VKEY_DOWN, true},
      {DomCode::NUMPAD3, VKEY_NEXT, true},
      {DomCode::NUMPAD4, VKEY_LEFT, true},
      {DomCode::NUMPAD5, VKEY_CLEAR, true},
      {DomCode::NUMPAD6, VKEY_RIGHT, true},
      {DomCode::NUMPAD7, VKEY_HOME, true},
      {DomCode::NUMPAD8, VKEY_UP, true},
      {DomCode::NUMPAD9, VKEY_PRIOR, true},
  };

  for (const auto& test_case : kTesCases) {
    KeyEvent event(EventType::kKeyPressed, test_case.ui_keycode,
                   test_case.dom_code, EF_NONE);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    EXPECT_EQ(test_case.expected_result, (webkit_event.GetModifiers() &
                                          blink::WebInputEvent::kIsKeyPad) != 0)
        << "Failed in "
        << "{dom_code:"
        << KeycodeConverter::DomCodeToCodeString(test_case.dom_code)
        << ", ui_keycode:" << test_case.ui_keycode
        << "}, expect: " << test_case.expected_result;
  }
}

TEST(WebInputEventTest, TestMakeWebMouseEvent) {
  {
    // Left pressed.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMousePressed, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_LEFT_MOUSE_BUTTON,
                        EF_LEFT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kLeft, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Left released.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMouseReleased, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, 0,
                        EF_LEFT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kLeft, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseUp, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Middle pressed.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMousePressed, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_MIDDLE_MOUSE_BUTTON,
                        EF_MIDDLE_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kMiddle, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Middle released.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMouseReleased, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, 0,
                        EF_MIDDLE_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kMiddle, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseUp, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Right pressed.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMousePressed, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_RIGHT_MOUSE_BUTTON,
                        EF_RIGHT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kRight, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Right released.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMouseReleased, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, 0,
                        EF_RIGHT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kRight, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseUp, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Moved
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMouseMoved, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, 0, 0);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kNoButton, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseMove, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Moved with left down
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMouseMoved, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_LEFT_MOUSE_BUTTON,
                        0);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kLeft, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseMove, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Left with shift pressed.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(
        EventType::kMousePressed, gfx::Point(123, 321), gfx::Point(123, 321),
        timestamp, EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN, EF_LEFT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kLeft, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown, webkit_event.GetType());
    EXPECT_EQ(ui_event.GetClickCount(), webkit_event.click_count);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Default values for PointerDetails.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseEvent ui_event(EventType::kMousePressed, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_LEFT_MOUSE_BUTTON,
                        EF_LEFT_MOUSE_BUTTON);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);

    EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
              webkit_event.pointer_type);
    EXPECT_EQ(0, webkit_event.tilt_x);
    EXPECT_EQ(0, webkit_event.tilt_y);
    EXPECT_TRUE(std::isnan(webkit_event.force));
    EXPECT_EQ(0.0f, webkit_event.tangential_pressure);
    EXPECT_EQ(0, webkit_event.twist);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
  {
    // Stylus values for PointerDetails.
    base::TimeTicks timestamp = EventTimeForNow();
    PointerDetails pointer_details(EventPointerType::kPen,
                                   /* id */ 63,
                                   /* radius_x */ 0.0f,
                                   /* radius_y */ 0.0f,
                                   /* force */ 0.8f,
                                   /* twist */ 269,
                                   /* tilt_x */ 89.5f,
                                   /* tilt_y */ -89.5f,
                                   /* tangential_pressure */ 0.6f);
    MouseEvent ui_event(EventType::kMousePressed, gfx::Point(123, 321),
                        gfx::Point(123, 321), timestamp, EF_LEFT_MOUSE_BUTTON,
                        EF_LEFT_MOUSE_BUTTON, pointer_details);
    blink::WebMouseEvent webkit_event = MakeWebMouseEvent(ui_event);

    EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
              webkit_event.pointer_type);
    EXPECT_EQ(89.5, webkit_event.tilt_x);
    EXPECT_EQ(-89.5, webkit_event.tilt_y);
    EXPECT_FLOAT_EQ(0.8f, webkit_event.force);
    EXPECT_FLOAT_EQ(0.6f, webkit_event.tangential_pressure);
    EXPECT_EQ(269, webkit_event.twist);
    EXPECT_EQ(63, webkit_event.id);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
}

TEST(WebInputEventTest, TestMakeWebMouseWheelEvent) {
  {
    // Mouse wheel.
    base::TimeTicks timestamp = EventTimeForNow();
    MouseWheelEvent ui_event(gfx::Vector2d(MouseWheelEvent::kWheelDelta * 2,
                                           -MouseWheelEvent::kWheelDelta * 2),
                             gfx::Point(123, 321), gfx::Point(123, 321),
                             timestamp, 0, 0);
    blink::WebMouseWheelEvent webkit_event = MakeWebMouseWheelEvent(ui_event);
    EXPECT_EQ(EventFlagsToWebEventModifiers(ui_event.flags()),
              webkit_event.GetModifiers());
    EXPECT_EQ(timestamp, webkit_event.TimeStamp());
    EXPECT_EQ(blink::WebMouseEvent::Button::kNoButton, webkit_event.button);
    EXPECT_EQ(blink::WebInputEvent::Type::kMouseWheel, webkit_event.GetType());
    EXPECT_FLOAT_EQ(ui_event.x_offset() / MouseWheelEvent::kWheelDelta,
                    webkit_event.wheel_ticks_x);
    EXPECT_FLOAT_EQ(ui_event.y_offset() / MouseWheelEvent::kWheelDelta,
                    webkit_event.wheel_ticks_y);
    EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
              webkit_event.pointer_type);
    EXPECT_EQ(0, webkit_event.tilt_x);
    EXPECT_EQ(0, webkit_event.tilt_y);
    EXPECT_TRUE(std::isnan(webkit_event.force));
    EXPECT_EQ(0.0f, webkit_event.tangential_pressure);
    EXPECT_EQ(0, webkit_event.twist);
    EXPECT_EQ(123, webkit_event.PositionInWidget().x());
    EXPECT_EQ(321, webkit_event.PositionInWidget().y());
  }
}

#if !BUILDFLAG(IS_MAC)
TEST(WebInputEventTest, TestPercentMouseWheelScroll) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWindowsScrollingPersonality);

  base::TimeTicks timestamp = EventTimeForNow();
  MouseWheelEvent ui_event(gfx::Vector2d(0, -MouseWheelEvent::kWheelDelta),
                           gfx::Point(123, 321), gfx::Point(123, 321),
                           timestamp, 0, 0);
  blink::WebMouseWheelEvent webkit_event = MakeWebMouseWheelEvent(ui_event);

  EXPECT_EQ(ui::ScrollGranularity::kScrollByPercentage,
            webkit_event.delta_units);
  EXPECT_FLOAT_EQ(0.f, webkit_event.delta_x);
  EXPECT_FLOAT_EQ(-0.05, webkit_event.delta_y);
  EXPECT_FLOAT_EQ(0.f, webkit_event.wheel_ticks_x);
  EXPECT_FLOAT_EQ(-1.f, webkit_event.wheel_ticks_y);
}
#endif

TEST(WebInputEventTest, KeyEvent) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  struct {
    ui::KeyEvent event;
    blink::WebInputEvent::Type web_type;
    int web_modifiers;
  } tests[] = {
      {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE),
       blink::WebInputEvent::Type::kRawKeyDown, 0x0},
      {ui::KeyEvent::FromCharacter(L'B', ui::VKEY_B, ui::DomCode::NONE,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
       blink::WebInputEvent::Type::kChar,
       blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kControlKey},
      {ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_C, ui::EF_ALT_DOWN),
       blink::WebInputEvent::Type::kKeyUp, blink::WebInputEvent::kAltKey}};

  for (size_t i = 0; i < std::size(tests); i++) {
    blink::WebKeyboardEvent web_event = MakeWebKeyboardEvent(tests[i].event);
    ASSERT_TRUE(blink::WebInputEvent::IsKeyboardEventType(web_event.GetType()));
    ASSERT_EQ(tests[i].web_type, web_event.GetType());
    ASSERT_EQ(tests[i].web_modifiers, web_event.GetModifiers());
    ASSERT_EQ(static_cast<int>(tests[i].event.GetLocatedWindowsKeyboardCode()),
              web_event.windows_key_code);
  }
}

TEST(WebInputEventTest, WheelEvent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kWindowsScrollingPersonality);
  const int kDeltaX = 14;
  const int kDeltaY = -3;
  ui::MouseWheelEvent ui_event(
      ui::MouseEvent(ui::EventType::kUnknown, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), 0, 0),
      kDeltaX, kDeltaY);
  blink::WebMouseWheelEvent web_event = MakeWebMouseWheelEvent(ui_event);
  ASSERT_EQ(blink::WebInputEvent::Type::kMouseWheel, web_event.GetType());
  ASSERT_EQ(0, web_event.GetModifiers());
  ASSERT_EQ(kDeltaX, web_event.delta_x);
  ASSERT_EQ(kDeltaY, web_event.delta_y);
}

TEST(WebInputEventTest, MousePointerEvent) {
  struct {
    ui::EventType ui_type;
    blink::WebInputEvent::Type web_type;
    int ui_modifiers;
    int web_modifiers;
    gfx::Point location;
    gfx::Point screen_location;
  } tests[] = {
      {ui::EventType::kMousePressed, blink::WebInputEvent::Type::kMouseDown,
       0x0, 0x0, gfx::Point(3, 5), gfx::Point(113, 125)},
      {ui::EventType::kMouseReleased, blink::WebInputEvent::Type::kMouseUp,
       ui::EF_LEFT_MOUSE_BUTTON, blink::WebInputEvent::kLeftButtonDown,
       gfx::Point(100, 1), gfx::Point(50, 1)},
      {ui::EventType::kMouseMoved, blink::WebInputEvent::Type::kMouseMove,
       ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
       blink::WebInputEvent::kMiddleButtonDown |
           blink::WebInputEvent::kRightButtonDown,
       gfx::Point(13, 3), gfx::Point(53, 3)},
  };

  for (size_t i = 0; i < std::size(tests); i++) {
    ui::MouseEvent ui_event(tests[i].ui_type, tests[i].location,
                            tests[i].screen_location, base::TimeTicks(),
                            tests[i].ui_modifiers, 0);
    blink::WebMouseEvent web_event = MakeWebMouseEvent(ui_event);
    ASSERT_TRUE(blink::WebInputEvent::IsMouseEventType(web_event.GetType()));
    ASSERT_EQ(tests[i].web_type, web_event.GetType());
    ASSERT_EQ(tests[i].web_modifiers, web_event.GetModifiers());
    ASSERT_EQ(tests[i].location.x(), web_event.PositionInWidget().x());
    ASSERT_EQ(tests[i].location.y(), web_event.PositionInWidget().y());
    ASSERT_EQ(tests[i].screen_location.x(), web_event.PositionInScreen().x());
    ASSERT_EQ(tests[i].screen_location.y(), web_event.PositionInScreen().y());
  }
}

#if BUILDFLAG(IS_WIN)
TEST(WebInputEventTest, MouseLeaveScreenCoordinate) {
  CHROME_MSG msg_event = {nullptr, WM_MOUSELEAVE, 0, MAKELPARAM(300, 200)};
  ::SetCursorPos(250, 350);
  ui::MouseEvent ui_event(msg_event);

  blink::WebMouseEvent web_event = MakeWebMouseEvent(ui_event);
  ASSERT_EQ(blink::WebInputEvent::Type::kMouseLeave, web_event.GetType());

  // WM_MOUSELEAVE events take coordinates from cursor position instead of
  // LPARAM.
  ASSERT_EQ(250, web_event.PositionInWidget().x());
  ASSERT_EQ(350, web_event.PositionInWidget().y());
  ASSERT_EQ(250, web_event.PositionInScreen().x());
  ASSERT_EQ(350, web_event.PositionInScreen().y());
}
#endif

TEST(WebInputEventTest, MouseMoveUnadjustedMovement) {
  gfx::PointF cursor_pos(123, 456);
  gfx::Vector2dF movement(-12, 34);
  ui::MouseEvent event(EventType::kMouseMoved, cursor_pos, cursor_pos,
                       base::TimeTicks(), 0, 0);
  MouseEvent::DispatcherApi(&event).set_movement(movement);

  blink::WebMouseEvent web_event = MakeWebMouseEvent(event);

  ASSERT_TRUE(web_event.is_raw_movement_event);
  ASSERT_EQ(web_event.movement_x, movement.x());
  ASSERT_EQ(web_event.movement_y, movement.y());
}

}  // namespace ui
