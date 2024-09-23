// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <utility>

#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/ipc/ui_events_param_traits_macros.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/mojom/event.mojom.h"
#include "ui/events/mojom/event_mojom_traits.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"  // nogncheck
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"  // nogncheck
#endif

namespace ui {

namespace {

void ExpectTouchEventsEqual(const TouchEvent& expected,
                            const TouchEvent& actual) {
  EXPECT_EQ(expected.may_cause_scrolling(), actual.may_cause_scrolling());
  EXPECT_EQ(expected.hovering(), actual.hovering());
  EXPECT_EQ(expected.pointer_details(), actual.pointer_details());
}

void ExpectLocatedEventsEqual(const LocatedEvent& expected,
                              const LocatedEvent& actual) {
  EXPECT_EQ(expected.location_f(), actual.location_f());
  EXPECT_EQ(expected.root_location_f(), actual.root_location_f());
}

void ExpectMouseEventsEqual(const MouseEvent& expected,
                            const MouseEvent& actual) {
  EXPECT_EQ(expected.pointer_details(), actual.pointer_details());
  EXPECT_EQ(expected.changed_button_flags(), actual.changed_button_flags());
}

void ExpectMouseWheelEventsEqual(const MouseWheelEvent& expected,
                                 const MouseWheelEvent& actual) {
  EXPECT_EQ(expected.offset(), actual.offset());
  EXPECT_EQ(expected.tick_120ths(), actual.tick_120ths());
}

void ExpectKeyEventsEqual(const KeyEvent& expected, const KeyEvent& actual) {
  EXPECT_EQ(expected.GetCharacter(), actual.GetCharacter());
  EXPECT_EQ(expected.GetUnmodifiedText(), actual.GetUnmodifiedText());
  EXPECT_EQ(expected.GetText(), actual.GetText());
  EXPECT_EQ(expected.is_char(), actual.is_char());
  EXPECT_EQ(expected.is_repeat(), actual.is_repeat());
  EXPECT_EQ(expected.GetConflatedWindowsKeyCode(),
            actual.GetConflatedWindowsKeyCode());
  EXPECT_EQ(expected.code(), actual.code());
}

void ExpectEventsEqual(const Event& expected, const Event& actual) {
  EXPECT_EQ(expected.type(), actual.type());
  EXPECT_EQ(expected.time_stamp(), actual.time_stamp());
  EXPECT_EQ(expected.flags(), actual.flags());
  if (expected.IsLocatedEvent()) {
    ASSERT_TRUE(actual.IsLocatedEvent());
    ExpectLocatedEventsEqual(*expected.AsLocatedEvent(),
                             *actual.AsLocatedEvent());
  }
  if (expected.IsMouseEvent()) {
    ASSERT_TRUE(actual.IsMouseEvent());
    ExpectMouseEventsEqual(*expected.AsMouseEvent(), *actual.AsMouseEvent());
  }
  if (expected.IsMouseWheelEvent()) {
    ASSERT_TRUE(actual.IsMouseWheelEvent());
    ExpectMouseWheelEventsEqual(*expected.AsMouseWheelEvent(),
                                *actual.AsMouseWheelEvent());
  }
  if (expected.IsTouchEvent()) {
    ASSERT_TRUE(actual.IsTouchEvent());
    ExpectTouchEventsEqual(*expected.AsTouchEvent(), *actual.AsTouchEvent());
  }
  if (expected.IsKeyEvent()) {
    ASSERT_TRUE(actual.IsKeyEvent());
    ExpectKeyEventsEqual(*expected.AsKeyEvent(), *actual.AsKeyEvent());
  }
}

}  // namespace

TEST(StructTraitsTest, KeyEvent) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  const KeyEvent kTestData[] = {
      {EventType::kKeyPressed, VKEY_RETURN, EF_CONTROL_DOWN},
      {EventType::kKeyPressed, VKEY_MENU, EF_ALT_DOWN},
      {EventType::kKeyReleased, VKEY_SHIFT, EF_SHIFT_DOWN},
      {EventType::kKeyReleased, VKEY_MENU, EF_ALT_DOWN},
      {EventType::kKeyPressed, VKEY_A, DomCode::US_A, EF_NONE},
      {EventType::kKeyPressed, VKEY_B, DomCode::US_B,
       EF_CONTROL_DOWN | EF_ALT_DOWN},
      ui::KeyEvent::FromCharacter('\x12', VKEY_2, DomCode::NONE,
                                  EF_CONTROL_DOWN),
      ui::KeyEvent::FromCharacter('Z', VKEY_Z, DomCode::NONE, EF_CAPS_LOCK_ON),
      ui::KeyEvent::FromCharacter('z', VKEY_Z, DomCode::NONE, EF_NONE),
      {EventType::kKeyPressed, VKEY_Z, EF_NONE,
       base::TimeTicks() + base::Microseconds(101)},
      ui::KeyEvent::FromCharacter('Z', VKEY_Z, DomCode::NONE, EF_NONE,
                                  base::TimeTicks() + base::Microseconds(102)),
  };

  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    EXPECT_TRUE(output->IsKeyEvent());

    const KeyEvent* output_key_event = output->AsKeyEvent();
    ExpectEventsEqual(kTestData[i], *output_key_event);
  }
}

TEST(StructTraitsTest, MouseEvent) {
  const MouseEvent kTestData[] = {
      {EventType::kMousePressed, gfx::Point(10, 10), gfx::Point(20, 30),
       base::TimeTicks() + base::Microseconds(201), EF_NONE, 0,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseDragged, gfx::Point(1, 5), gfx::Point(5, 1),
       base::TimeTicks() + base::Microseconds(202), EF_LEFT_MOUSE_BUTTON,
       EF_LEFT_MOUSE_BUTTON,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseReleased, gfx::Point(411, 130), gfx::Point(20, 30),
       base::TimeTicks() + base::Microseconds(203),
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseMoved, gfx::Point(0, 1), gfx::Point(2, 3),
       base::TimeTicks() + base::Microseconds(204), EF_ALT_DOWN, 0,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseEntered, gfx::Point(6, 7), gfx::Point(8, 9),
       base::TimeTicks() + base::Microseconds(205), EF_NONE, 0,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseExited, gfx::Point(10, 10), gfx::Point(20, 30),
       base::TimeTicks() + base::Microseconds(206), EF_BACK_MOUSE_BUTTON, 0,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
      {EventType::kMouseCaptureChanged, gfx::Point(99, 99), gfx::Point(99, 99),
       base::TimeTicks() + base::Microseconds(207), EF_CONTROL_DOWN, 0,
       PointerDetails(EventPointerType::kMouse, kPointerIdMouse)},
  };

  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    ASSERT_TRUE(output->IsMouseEvent());

    ExpectEventsEqual(kTestData[i], *output);
  }
}

TEST(StructTraitsTest, MouseWheelEvent) {
  const MouseWheelEvent kTestData[] = {
      {gfx::Vector2d(11, 15), gfx::PointF(3, 4), gfx::PointF(40, 30),
       base::TimeTicks() + base::Microseconds(301), EF_LEFT_MOUSE_BUTTON,
       EF_LEFT_MOUSE_BUTTON, gfx::Vector2d(1320, 1800)},
      {gfx::Vector2d(-5, 3), gfx::PointF(40, 3), gfx::PointF(4, 0),
       base::TimeTicks() + base::Microseconds(302),
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
       gfx::Vector2d(-600, 360)},
      {gfx::Vector2d(1, 0), gfx::PointF(3, 4), gfx::PointF(40, 30),
       base::TimeTicks() + base::Microseconds(303), EF_NONE, EF_NONE,
       gfx::Vector2d(120, -15)},
  };

  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    ASSERT_EQ(EventType::kMousewheel, output->type());

    const MouseWheelEvent* output_event = output->AsMouseWheelEvent();
    // TODO(sky): make this use ExpectEventsEqual().
    ExpectMouseWheelEventsEqual(kTestData[i], *output_event);
  }
}

TEST(StructTraitsTest, FloatingPointLocations) {
  // Create some events with non-integer locations.
  const gfx::PointF location(11.1, 22.2);
  const gfx::PointF root_location(33.3, 44.4);
  const base::TimeTicks time_stamp = base::TimeTicks::Now();
  MouseEvent mouse_event(EventType::kMousePressed, location, root_location,
                         time_stamp, EF_NONE, EF_NONE);
  MouseWheelEvent wheel_event(gfx::Vector2d(1, 0), location, root_location,
                              time_stamp, EF_NONE, EF_NONE);
  ScrollEvent scroll_event(EventType::kScroll, location, root_location,
                           time_stamp, EF_NONE, 1, 2, 3, 4, 5);
  TouchEvent touch_event(EventType::kTouchPressed, location, root_location,
                         time_stamp, {}, EF_NONE);
  Event* test_data[] = {&mouse_event, &wheel_event, &scroll_event,
                        &touch_event};

  // Serialize and deserialize does not round or truncate the locations.
  for (Event* event : test_data) {
    std::unique_ptr<Event> event_copy = event->Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Event>(event_copy, output));
    EXPECT_EQ(location, output->AsLocatedEvent()->location_f());
    EXPECT_EQ(root_location, output->AsLocatedEvent()->root_location_f());
  }
}

TEST(StructTraitsTest, KeyEventPropertiesSerialized) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  KeyEvent key_event(EventType::kKeyPressed, VKEY_T, EF_NONE);
  const std::string key = "key";
  const std::vector<uint8_t> value(0xCD, 2);
  KeyEvent::Properties properties;
  properties[key] = value;
  key_event.SetProperties(properties);

  std::unique_ptr<Event> event_ptr = key_event.Clone();
  std::unique_ptr<Event> deserialized;
  ASSERT_TRUE(mojom::Event::Deserialize(mojom::Event::Serialize(&event_ptr),
                                        &deserialized));
  ASSERT_TRUE(deserialized->IsKeyEvent());
  ASSERT_TRUE(deserialized->AsKeyEvent()->properties());
  EXPECT_EQ(properties, *(deserialized->AsKeyEvent()->properties()));
}

TEST(StructTraitsTest, GestureEvent) {
  GestureEventDetails pinch_begin_details(EventType::kGesturePinchBegin);
  pinch_begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  GestureEventDetails pinch_end_details(EventType::kGesturePinchEnd);
  pinch_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  GestureEventDetails pinch_update_details(EventType::kGesturePinchUpdate);
  pinch_update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  pinch_update_details.set_scale(1.23f);
  GestureEventDetails swipe_top_left_details(EventType::kGestureSwipe, -1, -1);
  swipe_top_left_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHPAD);
  GestureEventDetails swipe_right_details(EventType::kGestureSwipe, 1, 0);
  swipe_right_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);

  const GestureEvent kTestData[] = {
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       GestureEventDetails(EventType::kScrollFlingStart)},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       GestureEventDetails(EventType::kGestureTap)},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       pinch_begin_details},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       pinch_end_details},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       pinch_update_details},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       swipe_top_left_details},
      {10, 20, EF_NONE, base::TimeTicks() + base::Microseconds(401),
       swipe_right_details},
  };

  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    ASSERT_TRUE(output->IsGestureEvent());

    const GestureEvent* output_ptr_event = output->AsGestureEvent();
    ExpectEventsEqual(kTestData[i], *output);
    EXPECT_EQ(kTestData[i].details(), output_ptr_event->details());
    EXPECT_EQ(kTestData[i].unique_touch_event_id(),
              output_ptr_event->unique_touch_event_id());
  }
}

TEST(StructTraitsTest, ScrollEvent) {
  const ScrollEvent kTestData[] = {
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::NONE, ScrollEventPhase::kNone},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::NONE, ScrollEventPhase::kUpdate},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::NONE, ScrollEventPhase::kBegan},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::NONE, ScrollEventPhase::kEnd},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::BEGAN, ScrollEventPhase::kNone},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::INERTIAL_UPDATE, ScrollEventPhase::kNone},
      {EventType::kScroll, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(501), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::END, ScrollEventPhase::kNone},
      {EventType::kScrollFlingStart, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(502), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::MAY_BEGIN, ScrollEventPhase::kNone},
      {EventType::kScrollFlingCancel, gfx::Point(10, 20),
       base::TimeTicks() + base::Microseconds(502), EF_NONE, 1, 2, 3, 4, 5,
       EventMomentumPhase::END, ScrollEventPhase::kNone},
  };

  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    EXPECT_TRUE(output->IsScrollEvent());

    const ScrollEvent* output_ptr_event = output->AsScrollEvent();
    ExpectEventsEqual(kTestData[i], *output_ptr_event);
    EXPECT_EQ(kTestData[i].x_offset(), output_ptr_event->x_offset());
    EXPECT_EQ(kTestData[i].y_offset(), output_ptr_event->y_offset());
    EXPECT_EQ(kTestData[i].x_offset_ordinal(),
              output_ptr_event->x_offset_ordinal());
    EXPECT_EQ(kTestData[i].y_offset_ordinal(),
              output_ptr_event->y_offset_ordinal());
    EXPECT_EQ(kTestData[i].finger_count(), output_ptr_event->finger_count());
    EXPECT_EQ(kTestData[i].momentum_phase(),
              output_ptr_event->momentum_phase());
  }
}

TEST(StructTraitsTest, PointerDetails) {
  const PointerDetails kTestData[] = {
      {EventPointerType::kUnknown, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::kMouse, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::kPen, 11, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f},
      {EventPointerType::kTouch, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::kEraser, 21, 22.f, 23.f, 24.f, 25.f, 26.f, 27.f},
  };
  for (size_t i = 0; i < std::size(kTestData); i++) {
    // Set |offset| as the constructor used above does not modify it.
    PointerDetails input(kTestData[i]);
    input.offset.set_x(i);
    input.offset.set_y(i + 1);

    PointerDetails output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PointerDetails>(
        input, output));
    EXPECT_EQ(input, output);
  }
}

TEST(StructTraitsTest, TouchEvent) {
  const TouchEvent kTestData[] = {
      {EventType::kTouchReleased,
       {1, 2},
       base::TimeTicks::Now(),
       {EventPointerType::kUnknown, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
       EF_SHIFT_DOWN},
      {EventType::kTouchPressed,
       {1, 2},
       base::TimeTicks::Now(),
       {},
       EF_CONTROL_DOWN},
      {EventType::kTouchMoved, {1, 2}, base::TimeTicks::Now(), {}, EF_NONE},
      {EventType::kTouchCancelled, {1, 2}, base::TimeTicks::Now(), {}, EF_NONE},
  };
  for (size_t i = 0; i < std::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = kTestData[i].Clone();
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(expected_copy,
                                                                  output));
    ExpectEventsEqual(*expected_copy, *output);
  }

  // Serialize/Deserialize with fields that can not be set from constructor.
  std::unique_ptr<TouchEvent> touch_event =
      std::make_unique<TouchEvent>(EventType::kTouchCancelled, gfx::Point(),
                                   base::TimeTicks::Now(), PointerDetails());
  touch_event->set_may_cause_scrolling(true);
  touch_event->set_hovering(true);
  std::unique_ptr<Event> expected = std::move(touch_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(expected, output));
  ExpectEventsEqual(*expected, *output);
}

TEST(StructTraitsTest, UnserializedTouchEventFields) {
  std::unique_ptr<TouchEvent> touch_event =
      std::make_unique<TouchEvent>(EventType::kTouchCancelled, gfx::Point(),
                                   base::TimeTicks::Now(), PointerDetails());
  std::unique_ptr<Event> expected = std::move(touch_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(expected, output));
  ExpectEventsEqual(*expected, *output);
  EXPECT_NE(expected->AsTouchEvent()->unique_event_id(),
            output->AsTouchEvent()->unique_event_id());
}

#if BUILDFLAG(IS_OZONE)

// Test KeyboardLayoutEngine implementation that always returns 'x'.
class FixedKeyboardLayoutEngine : public StubKeyboardLayoutEngine {
 public:
  FixedKeyboardLayoutEngine() = default;

  FixedKeyboardLayoutEngine(const FixedKeyboardLayoutEngine&) = delete;
  FixedKeyboardLayoutEngine& operator=(const FixedKeyboardLayoutEngine&) =
      delete;

  ~FixedKeyboardLayoutEngine() override = default;

  // StubKeyboardLayoutEngine:
  bool Lookup(DomCode dom_code,
              int flags,
              DomKey* out_dom_key,
              KeyboardCode* out_key_code) const override {
    *out_dom_key = DomKey::FromCharacter('x');
    *out_key_code = ui::VKEY_X;
    return true;
  }
};

TEST(StructTraitsTest, DifferentKeyboardLayout) {
  // Verifies KeyEvent serialization is not impacted by a KeyboardLayoutEngine.
  ScopedKeyboardLayoutEngine scoped_keyboard_layout_engine(
      std::make_unique<FixedKeyboardLayoutEngine>());
  std::unique_ptr<KeyEvent> key_event = std::make_unique<KeyEvent>(
      EventType::kKeyPressed, VKEY_S, DomCode::US_S, EF_NONE,
      DomKey::FromCharacter('s'), base::TimeTicks::Now());
  std::unique_ptr<Event> expected = std::move(key_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(expected, output));
  ExpectEventsEqual(*expected, *output);
}

#endif

}  // namespace ui
