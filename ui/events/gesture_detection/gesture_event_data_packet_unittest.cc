// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data_packet.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const float kTouchX = 13.7f;
const float kTouchY = 14.2f;
const uint32_t uniqueTouchEventId = 1234U;

GestureEventData CreateGesture(EventType type) {
  return GestureEventData(GestureEventDetails(type), 0,
                          MotionEvent::ToolType::FINGER, base::TimeTicks(),
                          kTouchX, kTouchY, kTouchX + 5.f, kTouchY + 10.f, 1,
                          gfx::RectF(kTouchX - 1.f, kTouchY - 1.f, 2.f, 2.f),
                          EF_NONE, uniqueTouchEventId);
}

}  // namespace

bool GestureEqualsExceptForTouchId(const GestureEventData& lhs,
                                   const GestureEventData& rhs) {
  return lhs.type() == rhs.type() &&
         lhs.motion_event_id == rhs.motion_event_id &&
         lhs.primary_tool_type == rhs.primary_tool_type &&
         lhs.time == rhs.time && lhs.x == rhs.x && lhs.y == rhs.y &&
         lhs.raw_x == rhs.raw_x && lhs.raw_y == rhs.raw_y;
}

bool PacketEquals(const GestureEventDataPacket& lhs,
                  const GestureEventDataPacket& rhs) {
  if (lhs.timestamp() != rhs.timestamp() ||
      lhs.gesture_count() != rhs.gesture_count() ||
      lhs.timestamp() != rhs.timestamp() ||
      lhs.gesture_source() != rhs.gesture_source() ||
      lhs.touch_location() != rhs.touch_location() ||
      lhs.raw_touch_location() != rhs.raw_touch_location() ||
      lhs.unique_touch_event_id() != rhs.unique_touch_event_id())
    return false;

  for (size_t i = 0; i < lhs.gesture_count(); ++i) {
    if (!GestureEqualsExceptForTouchId(lhs.gesture(i), rhs.gesture(i)))
      return false;
    if (lhs.gesture(i).unique_touch_event_id !=
        rhs.gesture(i).unique_touch_event_id)
      return false;
  }

  return true;
}

class GestureEventDataPacketTest : public testing::Test {};

TEST_F(GestureEventDataPacketTest, Basic) {
  base::TimeTicks touch_time = base::TimeTicks::Now();

  GestureEventDataPacket packet;
  EXPECT_EQ(0U, packet.gesture_count());
  EXPECT_EQ(GestureEventDataPacket::UNDEFINED, packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::DOWN, touch_time, kTouchX, kTouchY));
  EXPECT_TRUE(touch_time == packet.timestamp());
  EXPECT_EQ(0U, packet.gesture_count());
  EXPECT_EQ(gfx::PointF(kTouchX, kTouchY), packet.touch_location());

  for (size_t i = static_cast<size_t>(EventType::kGestureTypeStart);
       i < static_cast<size_t>(EventType::kGestureTypeEnd); ++i) {
    const EventType type = static_cast<EventType>(i);
    GestureEventData gesture = CreateGesture(type);
    packet.Push(gesture);
    const size_t index = i - static_cast<int>(EventType::kGestureTypeStart);
    ASSERT_EQ(index + 1U, packet.gesture_count());
    EXPECT_TRUE(GestureEqualsExceptForTouchId(gesture, packet.gesture(index)));
    EXPECT_EQ(packet.unique_touch_event_id(),
              packet.gesture(index).unique_touch_event_id);
  }
}

TEST_F(GestureEventDataPacketTest, Copy) {
  GestureEventDataPacket packet0 = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::UP));
  packet0.Push(CreateGesture(EventType::kGestureTapDown));
  packet0.Push(CreateGesture(EventType::kGestureScrollBegin));

  GestureEventDataPacket packet1 = packet0;
  EXPECT_TRUE(PacketEquals(packet0, packet1));

  packet0 = packet1;
  EXPECT_TRUE(PacketEquals(packet1, packet0));
}

TEST_F(GestureEventDataPacketTest, GestureSource) {
  GestureEventDataPacket packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::DOWN));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_SEQUENCE_START,
            packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::UP));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_SEQUENCE_END,
            packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::CANCEL));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_SEQUENCE_CANCEL,
            packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::MOVE));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_MOVE, packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::POINTER_DOWN));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_START, packet.gesture_source());

  packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::POINTER_UP));
  EXPECT_EQ(GestureEventDataPacket::TOUCH_END, packet.gesture_source());

  GestureEventData gesture = CreateGesture(EventType::kGestureTap);
  packet = GestureEventDataPacket::FromTouchTimeout(gesture);
  EXPECT_EQ(GestureEventDataPacket::TOUCH_TIMEOUT, packet.gesture_source());
  EXPECT_EQ(1U, packet.gesture_count());
  EXPECT_EQ(base::TimeTicks(), packet.timestamp());
  EXPECT_EQ(gfx::PointF(gesture.x, gesture.y), packet.touch_location());
}

TEST_F(GestureEventDataPacketTest, AddEventLatencyMetadataToGestures) {
  GestureEventDataPacket packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::DOWN));
  packet.Push(CreateGesture(EventType::kGestureTap));
  packet.Push(CreateGesture(EventType::kGestureScrollUpdate));
  packet.Push(CreateGesture(EventType::kGesturePinchUpdate));

  EventLatencyMetadata event_latency_metadata;
  event_latency_metadata.scrolls_blocking_touch_dispatched_to_renderer =
      base::TimeTicks::Now();
  packet.AddEventLatencyMetadataToGestures(
      event_latency_metadata,
      base::BindRepeating([](const ui::GestureEventData& data) {
        return data.type() == EventType::kGestureScrollUpdate;
      }));

  EXPECT_TRUE(packet.gesture(0)
                  .details.GetEventLatencyMetadata()
                  .scrolls_blocking_touch_dispatched_to_renderer.is_null());
  EXPECT_EQ(
      packet.gesture(1)
          .details.GetEventLatencyMetadata()
          .scrolls_blocking_touch_dispatched_to_renderer,
      event_latency_metadata.scrolls_blocking_touch_dispatched_to_renderer);
  EXPECT_TRUE(packet.gesture(2)
                  .details.GetEventLatencyMetadata()
                  .scrolls_blocking_touch_dispatched_to_renderer.is_null());
}

}  // namespace ui
