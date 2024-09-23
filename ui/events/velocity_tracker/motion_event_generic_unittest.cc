// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/velocity_tracker/motion_event_generic.h"

#include <numbers>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/motion_event_test_utils.h"

namespace ui {

TEST(MotionEventGenericTest, Basic) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  MotionEventGeneric event(MotionEvent::Action::DOWN, event_time,
                           PointerProperties());
  EXPECT_EQ(1U, event.GetPointerCount());
  EXPECT_EQ(0U, event.GetHistorySize());
  EXPECT_EQ(event_time, event.GetEventTime());

  DCHECK_NE(0U, event.GetUniqueEventId());
  event.set_unique_event_id(123456U);
  EXPECT_EQ(123456U, event.GetUniqueEventId());

  event.PushPointer(PointerProperties(8.3f, 4.7f, 0.9f));
  ASSERT_EQ(2U, event.GetPointerCount());
  EXPECT_EQ(8.3f, event.GetX(1));
  EXPECT_EQ(4.7f, event.GetY(1));

  event.PushPointer(PointerProperties(2.3f, -3.7f, 5.8f));
  ASSERT_EQ(3U, event.GetPointerCount());
  EXPECT_EQ(2.3f, event.GetX(2));
  EXPECT_EQ(-3.7f, event.GetY(2));

  event.pointer(0).id = 3;
  EXPECT_EQ(3, event.GetPointerId(0));

  event.set_action(MotionEvent::Action::POINTER_DOWN);
  EXPECT_EQ(MotionEvent::Action::POINTER_DOWN, event.GetAction());

  event_time += base::Milliseconds(5);
  event.set_event_time(event_time);
  EXPECT_EQ(event_time, event.GetEventTime());

  event.set_button_state(MotionEvent::BUTTON_PRIMARY);
  EXPECT_EQ(MotionEvent::BUTTON_PRIMARY, event.GetButtonState());

  event.set_flags(EF_ALT_DOWN | EF_SHIFT_DOWN);
  EXPECT_EQ(EF_ALT_DOWN | EF_SHIFT_DOWN, event.GetFlags());

  event.set_action_index(1);
  EXPECT_EQ(1, event.GetActionIndex());

  event.set_action(MotionEvent::Action::MOVE);
  EXPECT_EQ(MotionEvent::Action::MOVE, event.GetAction());

  PointerProperties historical_pointer0(1.2f, 2.4f, 1.f);
  PointerProperties historical_pointer1(2.4f, 4.8f, 2.f);
  PointerProperties historical_pointer2(4.8f, 9.6f, 3.f);
  MotionEventGeneric historical_event(MotionEvent::Action::MOVE,
                                      event_time - base::Milliseconds(5),
                                      historical_pointer0);
  historical_event.PushPointer(historical_pointer1);
  historical_event.PushPointer(historical_pointer2);

  event.PushHistoricalEvent(historical_event.Clone());
  EXPECT_EQ(1U, event.GetHistorySize());
  EXPECT_EQ(event_time - base::Milliseconds(5),
            event.GetHistoricalEventTime(0));
  EXPECT_EQ(1.2f, event.GetHistoricalX(0, 0));
  EXPECT_EQ(2.4f, event.GetHistoricalY(0, 0));
  EXPECT_EQ(1.f, event.GetHistoricalTouchMajor(0, 0));
  EXPECT_EQ(2.4f, event.GetHistoricalX(1, 0));
  EXPECT_EQ(4.8f, event.GetHistoricalY(1, 0));
  EXPECT_EQ(2.f, event.GetHistoricalTouchMajor(1, 0));
  EXPECT_EQ(4.8f, event.GetHistoricalX(2, 0));
  EXPECT_EQ(9.6f, event.GetHistoricalY(2, 0));
  EXPECT_EQ(3.f, event.GetHistoricalTouchMajor(2, 0));
}

TEST(MotionEventGenericTest, Clone) {
  MotionEventGeneric event(MotionEvent::Action::DOWN, base::TimeTicks::Now(),
                           PointerProperties(8.3f, 4.7f, 2.f));
  event.set_button_state(MotionEvent::BUTTON_PRIMARY);

  std::unique_ptr<MotionEvent> clone = event.Clone();
  ASSERT_TRUE(clone);
  EXPECT_EQ(event.GetUniqueEventId(), clone->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*clone));
}

TEST(MotionEventGenericTest, CloneWithHistory) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  base::TimeTicks historical_event_time = event_time - base::Milliseconds(5);

  PointerProperties pointer(8.3f, 4.7f, 10.1f);
  MotionEventGeneric event(MotionEvent::Action::MOVE, event_time, pointer);

  PointerProperties historical_pointer(3.4f, -4.3f, 11.5);
  std::unique_ptr<MotionEvent> historical_event(new MotionEventGeneric(
      MotionEvent::Action::MOVE, historical_event_time, historical_pointer));

  event.PushHistoricalEvent(std::move(historical_event));
  EXPECT_EQ(1U, event.GetHistorySize());

  std::unique_ptr<MotionEvent> clone = event.Clone();
  ASSERT_TRUE(clone);
  EXPECT_EQ(event.GetUniqueEventId(), clone->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*clone));
}

TEST(MotionEventGenericTest, Cancel) {
  MotionEventGeneric event(MotionEvent::Action::UP, base::TimeTicks::Now(),
                           PointerProperties(8.7f, 4.3f, 1.f));
  event.set_button_state(MotionEvent::BUTTON_SECONDARY);

  std::unique_ptr<MotionEvent> cancel = event.Cancel();
  event.set_action(MotionEvent::Action::CANCEL);
  ASSERT_TRUE(cancel);
  EXPECT_NE(event.GetUniqueEventId(), cancel->GetUniqueEventId());
  EXPECT_EQ(test::ToString(event), test::ToString(*cancel));
}

TEST(MotionEventGenericTest, FindPointerIndexOfId) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  PointerProperties pointer;
  pointer.id = 0;
  MotionEventGeneric event0(MotionEvent::Action::DOWN, event_time, pointer);
  EXPECT_EQ(0, event0.FindPointerIndexOfId(0));
  EXPECT_EQ(-1, event0.FindPointerIndexOfId(1));
  EXPECT_EQ(-1, event0.FindPointerIndexOfId(-1));

  MotionEventGeneric event1(event0);
  pointer.id = 7;
  EXPECT_EQ(1u, event1.PushPointer(pointer));
  EXPECT_EQ(0, event1.FindPointerIndexOfId(0));
  EXPECT_EQ(1, event1.FindPointerIndexOfId(7));
  EXPECT_EQ(-1, event1.FindPointerIndexOfId(6));
  EXPECT_EQ(-1, event1.FindPointerIndexOfId(1));

  MotionEventGeneric event2(event1);
  pointer.id = 3;
  EXPECT_EQ(2u, event2.PushPointer(pointer));
  EXPECT_EQ(0, event2.FindPointerIndexOfId(0));
  EXPECT_EQ(1, event2.FindPointerIndexOfId(7));
  EXPECT_EQ(2, event2.FindPointerIndexOfId(3));
  EXPECT_EQ(-1, event2.FindPointerIndexOfId(1));
  EXPECT_EQ(-1, event2.FindPointerIndexOfId(2));
}

TEST(MotionEventGenericTest, RemovePointerAt) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  PointerProperties pointer;
  pointer.id = 0;
  MotionEventGeneric event(MotionEvent::Action::DOWN, event_time, pointer);

  pointer.id = 7;
  EXPECT_EQ(1u, event.PushPointer(pointer));
  EXPECT_EQ(2u, event.GetPointerCount());

  // Remove from the end.
  event.RemovePointerAt(1);
  EXPECT_EQ(1u, event.GetPointerCount());
  EXPECT_EQ(-1, event.FindPointerIndexOfId(7));
  EXPECT_EQ(0, event.FindPointerIndexOfId(0));

  EXPECT_EQ(1u, event.PushPointer(pointer));
  EXPECT_EQ(2u, event.GetPointerCount());

  // Remove from the beginning.
  event.RemovePointerAt(0);
  EXPECT_EQ(1u, event.GetPointerCount());
  EXPECT_EQ(0, event.FindPointerIndexOfId(7));
  EXPECT_EQ(-1, event.FindPointerIndexOfId(0));
}

TEST(MotionEventGenericTest, AxisAndOrientation) {
  {
    PointerProperties properties;
    float radius_x = 10;
    float radius_y = 5;
    float rotation_angle_deg = 0;
    properties.SetAxesAndOrientation(radius_x, radius_y, rotation_angle_deg);
    EXPECT_EQ(20, properties.touch_major);
    EXPECT_EQ(10, properties.touch_minor);
    EXPECT_NEAR(-std::numbers::pi / 2, properties.orientation, 0.001);
  }
  {
    PointerProperties properties;
    float radius_x = 5;
    float radius_y = 10;
    float rotation_angle_deg = 0;
    properties.SetAxesAndOrientation(radius_x, radius_y, rotation_angle_deg);
    EXPECT_EQ(20, properties.touch_major);
    EXPECT_EQ(10, properties.touch_minor);
    EXPECT_NEAR(0, properties.orientation, 0.001);
  }
  {
    PointerProperties properties;
    float radius_x = 10;
    float radius_y = 5;
    float rotation_angle_deg = 179.99f;
    properties.SetAxesAndOrientation(radius_x, radius_y, rotation_angle_deg);
    EXPECT_EQ(20, properties.touch_major);
    EXPECT_EQ(10, properties.touch_minor);
    EXPECT_NEAR(std::numbers::pi / 2, properties.orientation, 0.001);
  }
  {
    PointerProperties properties;
    float radius_x = 10;
    float radius_y = 5;
    float rotation_angle_deg = 90;
    properties.SetAxesAndOrientation(radius_x, radius_y, rotation_angle_deg);
    EXPECT_EQ(20, properties.touch_major);
    EXPECT_EQ(10, properties.touch_minor);
    EXPECT_NEAR(0, properties.orientation, 0.001);
  }
}

TEST(MotionEventGenericTest, ToString) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  base::TimeTicks historical_event_time0 = event_time - base::Milliseconds(10);
  base::TimeTicks historical_event_time1 = event_time - base::Milliseconds(5);

  PointerProperties pointer0(1, 2, 3);
  pointer0.id = 7;
  pointer0.pressure = 10;
  pointer0.touch_minor = 15;
  pointer0.touch_major = 20;
  pointer0.orientation = 1;

  PointerProperties pointer1(4, 5, 6);
  pointer1.id = 3;
  pointer0.pressure = 25;
  pointer0.touch_minor = 30;
  pointer0.touch_major = 35;
  pointer0.orientation = -1;

  MotionEventGeneric event(MotionEvent::Action::MOVE, event_time, pointer0);
  event.PushPointer(pointer1);

  pointer0.x += 50;
  pointer1.x -= 50;
  std::unique_ptr<MotionEventGeneric> historical_event0(new MotionEventGeneric(
      MotionEvent::Action::MOVE, historical_event_time0, pointer0));
  historical_event0->PushPointer(pointer1);

  pointer0.x += 100;
  pointer1.x -= 100;
  std::unique_ptr<MotionEventGeneric> historical_event1(new MotionEventGeneric(
      MotionEvent::Action::MOVE, historical_event_time1, pointer0));
  historical_event1->PushPointer(pointer1);

  event.PushHistoricalEvent(std::move(historical_event0));
  event.PushHistoricalEvent(std::move(historical_event1));
  ASSERT_EQ(2U, event.GetHistorySize());
  ASSERT_EQ(2U, event.GetPointerCount());

  // Do a basic smoke exercise of event stringification to ensure things don't
  // explode in the process.
  std::string event_string = test::ToString(event);
  EXPECT_FALSE(event_string.empty());
}

}  // namespace ui
