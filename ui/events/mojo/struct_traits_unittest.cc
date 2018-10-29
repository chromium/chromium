// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/stl_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/mojo/event.mojom.h"
#include "ui/events/mojo/event_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/latency/mojo/latency_info_struct_traits.h"

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
}

}  // namespace

TEST(StructTraitsTest, KeyEvent) {
  const KeyEvent kTestData[] = {
      {ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN},
      {ET_KEY_PRESSED, VKEY_MENU, EF_ALT_DOWN},
      {ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN},
      {ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN},
      {ET_KEY_PRESSED, VKEY_A, DomCode::US_A, EF_NONE},
      {ET_KEY_PRESSED, VKEY_B, DomCode::US_B, EF_CONTROL_DOWN | EF_ALT_DOWN},
      {'\x12', VKEY_2, DomCode::NONE, EF_CONTROL_DOWN},
      {'Z', VKEY_Z, DomCode::NONE, EF_CAPS_LOCK_ON},
      {'z', VKEY_Z, DomCode::NONE, EF_NONE},
      {ET_KEY_PRESSED, VKEY_Z, EF_NONE,
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(101)},
      {'Z', VKEY_Z, DomCode::NONE, EF_NONE,
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(102)},
  };

  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
    EXPECT_TRUE(output->IsKeyEvent());

    const KeyEvent* output_key_event = output->AsKeyEvent();
    ExpectEventsEqual(kTestData[i], *output_key_event);
    EXPECT_EQ(kTestData[i].GetCharacter(), output_key_event->GetCharacter());
    EXPECT_EQ(kTestData[i].GetUnmodifiedText(),
              output_key_event->GetUnmodifiedText());
    EXPECT_EQ(kTestData[i].GetText(), output_key_event->GetText());
    EXPECT_EQ(kTestData[i].is_char(), output_key_event->is_char());
    EXPECT_EQ(kTestData[i].is_repeat(), output_key_event->is_repeat());
    EXPECT_EQ(kTestData[i].GetConflatedWindowsKeyCode(),
              output_key_event->GetConflatedWindowsKeyCode());
    EXPECT_EQ(kTestData[i].code(), output_key_event->code());
  }
}

TEST(StructTraitsTest, MouseEvent) {
  const MouseEvent kTestData[] = {
      {ET_MOUSE_PRESSED, gfx::Point(10, 10), gfx::Point(20, 30),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(201), EF_NONE, 0,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_DRAGGED, gfx::Point(1, 5), gfx::Point(5, 1),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(202),
       EF_LEFT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_RELEASED, gfx::Point(411, 130), gfx::Point(20, 30),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(203),
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_MOVED, gfx::Point(0, 1), gfx::Point(2, 3),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(204), EF_ALT_DOWN,
       0,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_ENTERED, gfx::Point(6, 7), gfx::Point(8, 9),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(205), EF_NONE, 0,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_EXITED, gfx::Point(10, 10), gfx::Point(20, 30),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(206),
       EF_BACK_MOUSE_BUTTON, 0,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
      {ET_MOUSE_CAPTURE_CHANGED, gfx::Point(99, 99), gfx::Point(99, 99),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(207),
       EF_CONTROL_DOWN, 0,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                      MouseEvent::kMousePointerId)},
  };

  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
    ASSERT_TRUE(output->IsMouseEvent());

    ExpectEventsEqual(kTestData[i], *output);
  }
}

TEST(StructTraitsTest, MouseWheelEvent) {
  const MouseWheelEvent kTestData[] = {
      {gfx::Vector2d(11, 15), gfx::Point(3, 4), gfx::Point(40, 30),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(301),
       EF_LEFT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON},
      {gfx::Vector2d(-5, 3), gfx::Point(40, 3), gfx::Point(4, 0),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(302),
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON},
      {gfx::Vector2d(1, 0), gfx::Point(3, 4), gfx::Point(40, 30),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(303), EF_NONE,
       EF_NONE},
  };

  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
    ASSERT_EQ(ET_MOUSEWHEEL, output->type());

    const MouseWheelEvent* output_event = output->AsMouseWheelEvent();
    // TODO(sky): make this use ExpectEventsEqual().
    ExpectMouseWheelEventsEqual(kTestData[i], *output_event);
  }
}

TEST(StructTraitsTest, FloatingPointLocations) {
  MouseEvent input_event(
      ET_MOUSE_PRESSED, gfx::Point(10, 10), gfx::Point(20, 30),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(201), EF_NONE, 0,
      PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                     MouseEvent::kMousePointerId));

  input_event.set_location_f(gfx::PointF(10.1, 10.2));
  input_event.set_root_location_f(gfx::PointF(20.2, 30.3));

  std::unique_ptr<Event> expected_copy = Event::Clone(input_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(&expected_copy,
                                                                &output));
  ASSERT_TRUE(output->IsMouseEvent());

  ExpectEventsEqual(input_event, *output->AsMouseEvent());
}

TEST(StructTraitsTest, KeyEventPropertiesSerialized) {
  KeyEvent key_event(ET_KEY_PRESSED, VKEY_T, EF_NONE);
  const std::string key = "key";
  const std::vector<uint8_t> value(0xCD, 2);
  KeyEvent::Properties properties;
  properties[key] = value;
  key_event.SetProperties(properties);

  std::unique_ptr<Event> event_ptr = Event::Clone(key_event);
  std::unique_ptr<Event> deserialized;
  ASSERT_TRUE(mojom::Event::Deserialize(mojom::Event::Serialize(&event_ptr),
                                        &deserialized));
  ASSERT_TRUE(deserialized->IsKeyEvent());
  ASSERT_TRUE(deserialized->AsKeyEvent()->properties());
  EXPECT_EQ(properties, *(deserialized->AsKeyEvent()->properties()));
}

TEST(StructTraitsTest, GestureEvent) {
  const GestureEvent kTestData[] = {
      {10, 20, EF_NONE,
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(401),
       GestureEventDetails(ET_SCROLL_FLING_START)},
      {10, 20, EF_NONE,
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(401),
       GestureEventDetails(ET_GESTURE_TAP)},
  };

  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
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
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::NONE, ScrollEventPhase::kNone},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::NONE, ScrollEventPhase::kUpdate},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::NONE, ScrollEventPhase::kBegan},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::NONE, ScrollEventPhase::kEnd},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::BEGAN, ScrollEventPhase::kNone},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::INERTIAL_UPDATE,
       ScrollEventPhase::kNone},
      {ET_SCROLL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(501), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::END, ScrollEventPhase::kNone},
      {ET_SCROLL_FLING_START, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(502), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::MAY_BEGIN, ScrollEventPhase::kNone},
      {ET_SCROLL_FLING_CANCEL, gfx::Point(10, 20),
       base::TimeTicks() + base::TimeDelta::FromMicroseconds(502), EF_NONE, 1,
       2, 3, 4, 5, EventMomentumPhase::END, ScrollEventPhase::kNone},
  };

  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
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
      {EventPointerType::POINTER_TYPE_UNKNOWN, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::POINTER_TYPE_MOUSE, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::POINTER_TYPE_PEN, 11, 12.f, 13.f, 14.f, 15.f, 16.f,
       17.f},
      {EventPointerType::POINTER_TYPE_TOUCH, 1, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
      {EventPointerType::POINTER_TYPE_ERASER, 21, 22.f, 23.f, 24.f, 25.f, 26.f,
       27.f},
  };
  for (size_t i = 0; i < base::size(kTestData); i++) {
    // Set |offset| as the constructor used above does not modify it.
    PointerDetails input(kTestData[i]);
    input.offset.set_x(i);
    input.offset.set_y(i + 1);

    PointerDetails output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PointerDetails>(
        &input, &output));
    EXPECT_EQ(input, output);
  }
}

TEST(StructTraitsTest, TouchEvent) {
  const TouchEvent kTestData[] = {
      {ET_TOUCH_RELEASED,
       {1, 2},
       base::TimeTicks::Now(),
       {EventPointerType::POINTER_TYPE_UNKNOWN, 1, 2.f, 3.f, 4.f, 5.f, 6.f,
        7.f},
       EF_SHIFT_DOWN},
      {ET_TOUCH_PRESSED, {1, 2}, base::TimeTicks::Now(), {}, EF_CONTROL_DOWN},
      {ET_TOUCH_MOVED, {1, 2}, base::TimeTicks::Now(), {}, EF_NONE},
      {ET_TOUCH_CANCELLED, {1, 2}, base::TimeTicks::Now(), {}, EF_NONE},
  };
  for (size_t i = 0; i < base::size(kTestData); i++) {
    std::unique_ptr<Event> expected_copy = Event::Clone(kTestData[i]);
    std::unique_ptr<Event> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(
        &expected_copy, &output));
    ExpectEventsEqual(*expected_copy, *output);
  }

  // Serialize/Deserialize with fields that can not be set from constructor.
  std::unique_ptr<TouchEvent> touch_event =
      std::make_unique<TouchEvent>(ET_TOUCH_CANCELLED, gfx::Point(),
                                   base::TimeTicks::Now(), PointerDetails());
  touch_event->set_may_cause_scrolling(true);
  touch_event->set_hovering(true);
  std::unique_ptr<Event> expected = std::move(touch_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(&expected, &output));
  ExpectEventsEqual(*expected, *output);
}

TEST(StructTraitsTest, UnserializedTouchEventFields) {
  std::unique_ptr<TouchEvent> touch_event =
      std::make_unique<TouchEvent>(ET_TOUCH_CANCELLED, gfx::Point(),
                                   base::TimeTicks::Now(), PointerDetails());
  touch_event->set_should_remove_native_touch_id_mapping(true);
  std::unique_ptr<Event> expected = std::move(touch_event);
  std::unique_ptr<Event> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(&expected, &output));
  ExpectEventsEqual(*expected, *output);
  // Have to set this back to false, else the destructor tries to access
  // state not setup in tests.
  expected->AsTouchEvent()->set_should_remove_native_touch_id_mapping(false);
  // See comments in TouchEvent for why these two fields are not persisted.
  EXPECT_FALSE(output->AsTouchEvent()->should_remove_native_touch_id_mapping());
  EXPECT_NE(expected->AsTouchEvent()->unique_event_id(),
            output->AsTouchEvent()->unique_event_id());
}

}  // namespace ui
