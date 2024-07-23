// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/events/types/event_type.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const int kDefaultEventFlags = EF_ALT_DOWN | EF_SHIFT_DOWN;

}  // namespace

class TouchDispositionGestureFilterTest
    : public testing::Test,
      public TouchDispositionGestureFilterClient {
 public:
  TouchDispositionGestureFilterTest()
      : cancel_after_next_gesture_(false), sent_gesture_count_(0) {}
  ~TouchDispositionGestureFilterTest() override {}

  // testing::Test
  void SetUp() override {
    queue_ = std::make_unique<TouchDispositionGestureFilter>(this);
    touch_event_.set_flags(kDefaultEventFlags);
  }

  void TearDown() override { queue_.reset(); }

  // TouchDispositionGestureFilterClient
  void ForwardGestureEvent(const GestureEventData& event) override {
    EXPECT_EQ(GestureDeviceType::DEVICE_TOUCHSCREEN,
              event.details.device_type());
    ++sent_gesture_count_;
    last_sent_gesture_ = std::make_unique<GestureEventData>(event);
    sent_gestures_.push_back(event.type());
    if (event.type() == EventType::kGestureShowPress) {
      show_press_bounding_box_ = event.details.bounding_box();
    }
    if (cancel_after_next_gesture_) {
      cancel_after_next_gesture_ = false;
      SendPacket(CancelTouchPoint(), NoGestures());
      SendTouchNotConsumedAckForLastTouch();
    }
  }

 protected:
  using GestureList = std::vector<EventType>;

  ::testing::AssertionResult GesturesMatch(const GestureList& expected,
                                           const GestureList& actual) {
    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
          << "actual.size(" << actual.size()
          << ") != expected.size(" << expected.size() << ")";
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
               << "actual[" << i << "] (" << base::to_underlying(actual[i])
               << ") != expected[" << i << "] ("
               << base::to_underlying(expected[i]) << ")";
      }
    }

    return ::testing::AssertionSuccess();
  }

  GestureList NoGestures() { return GestureList(0); }

  GestureList Gestures(EventType type) {
    return GestureList(1, type);
  }

  GestureList Gestures(EventType type0, EventType type1) {
    GestureList gestures(2);
    gestures[0] = type0;
    gestures[1] = type1;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2) {
    GestureList gestures(3);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2,
                       EventType type3) {
    GestureList gestures(4);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    gestures[3] = type3;
    return gestures;
  }

  TouchDispositionGestureFilter::PacketResult SendTouchGesturesWithResult(
      const MotionEvent& touch,
      const GestureEventDataPacket& packet) {
    GestureEventDataPacket touch_packet =
        GestureEventDataPacket::FromTouch(touch);
    for (size_t i = 0; i < packet.gesture_count(); ++i)
      touch_packet.Push(packet.gesture(i));
    return queue_->OnGesturePacket(touch_packet);
  }

  uint32_t SendTouchGestures(const MotionEvent& touch,
                             const GestureEventDataPacket& packet) {
    SendTouchGesturesWithResult(touch, packet);
    last_sent_touch_event_id_ = touch.GetUniqueEventId();
    return touch.GetUniqueEventId();
  }

  TouchDispositionGestureFilter::PacketResult
  SendTimeoutGesture(EventType type) {
    return queue_->OnGesturePacket(
        GestureEventDataPacket::FromTouchTimeout(CreateGesture(type)));
  }

  TouchDispositionGestureFilter::PacketResult
  SendGesturePacket(const GestureEventDataPacket& packet) {
    return queue_->OnGesturePacket(packet);
  }

  void SendTouchEventAck(uint32_t touch_event_id,
                         bool event_consumed,
                         bool is_source_touch_event_set_blocking) {
    queue_->OnTouchEventAck(touch_event_id, event_consumed,
                            is_source_touch_event_set_blocking);
  }

  void SendTouchConsumedAck(uint32_t touch_event_id) {
    SendTouchEventAck(touch_event_id, true /* event_consumed */,
                      false /* is_source_touch_event_set_blocking */);
  }

  void SendTouchNotConsumedAck(uint32_t touch_event_id) {
    SendTouchEventAck(touch_event_id, false /* event_consumed */,
                      false /* is_source_touch_event_set_blocking */);
  }

  void SendTouchConsumedAckForLastTouch() {
    SendTouchEventAck(last_sent_touch_event_id_, true /* event_consumed */,
                      false /* is_source_touch_event_set_blocking */);
  }

  void SendTouchNotConsumedAckForLastTouch() {
    SendTouchEventAck(last_sent_touch_event_id_, false /* event_consumed */,
                      false /* is_source_touch_event_set_blocking */);
  }

  void PushGesture(EventType type) {
    pending_gesture_packet_.Push(CreateGesture(type));
  }

  void PushGesture(EventType type, float x, float y, float diameter) {
    pending_gesture_packet_.Push(CreateGesture(type, x, y, diameter));
  }

  const MockMotionEvent& PressTouchPoint(float x, float y) {
    touch_event_.PressPoint(x, y);
    touch_event_.SetRawOffset(raw_offset_.x(), raw_offset_.y());
    return touch_event_;
  }

  const MockMotionEvent& PressTouchPoint() { return PressTouchPoint(0, 0); }

  const MockMotionEvent& MoveTouchPoint() {
    touch_event_.MovePoint(0, 0, 0);
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  const MockMotionEvent& ReleaseTouchPoint() {
    touch_event_.ReleasePoint();
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  const MockMotionEvent& CancelTouchPoint() {
    touch_event_.CancelPoint();
    touch_event_.set_event_time(base::TimeTicks::Now());
    return touch_event_;
  }

  uint32_t SendPacket(const MockMotionEvent& touch_event,
                      GestureList gesture_list) {
    GestureEventDataPacket gesture_packet;
    for (EventType type : gesture_list)
      gesture_packet.Push(CreateGesture(type));
    last_sent_touch_event_id_ = touch_event.GetUniqueEventId();
    EXPECT_EQ(TouchDispositionGestureFilter::SUCCESS,
              SendTouchGesturesWithResult(touch_event, gesture_packet));
    return touch_event.GetUniqueEventId();
  }

  void SetRawTouchOffset(const gfx::Vector2dF& raw_offset) {
    raw_offset_ = raw_offset;
  }

  const MockMotionEvent& ResetTouchPoints() {
    touch_event_ = MockMotionEvent();
    return touch_event_;
  }

  bool GesturesSent() const { return !sent_gestures_.empty(); }

  const GestureEventData& last_sent_gesture() const {
    CHECK(last_sent_gesture_);
    return *last_sent_gesture_;
  }

  base::TimeTicks LastSentGestureTime() const {
    return last_sent_gesture().time;
  }

  base::TimeTicks CurrentTouchTime() const {
    return touch_event_.GetEventTime();
  }

  bool IsEmpty() const { return queue_->IsEmpty(); }

  GestureList GetAndResetSentGestures() {
    GestureList sent_gestures;
    sent_gestures.swap(sent_gestures_);
    return sent_gestures;
  }

  gfx::PointF LastSentGestureLocation() const {
    return gfx::PointF(last_sent_gesture().x, last_sent_gesture().y);
  }

  gfx::PointF LastSentGestureRawLocation() const {
    return gfx::PointF(last_sent_gesture().raw_x, last_sent_gesture().raw_y);
  }

  int LastSentGestureFlags() const {
    return last_sent_gesture().flags;
  }

  const gfx::Rect& ShowPressBoundingBox() const {
    return show_press_bounding_box_;
  }

  void SetCancelAfterNextGesture(bool cancel_after_next_gesture) {
    cancel_after_next_gesture_ = cancel_after_next_gesture;
  }

  GestureEventData CreateGesture(EventType type) {
    return CreateGesture(type, 0, 0, 0);
  }

  GestureEventData CreateGesture(EventType type,
                                 float x,
                                 float y,
                                 float diameter) {
    GestureEventDetails details(type);
    details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    return GestureEventData(
        details, 0, MotionEvent::ToolType::FINGER, base::TimeTicks(), x, y, 0,
        0, 1,
        gfx::RectF(x - diameter / 2, y - diameter / 2, diameter, diameter),
        kDefaultEventFlags, 0U);
  }

 private:
  std::unique_ptr<TouchDispositionGestureFilter> queue_;
  bool cancel_after_next_gesture_;
  MockMotionEvent touch_event_;
  GestureEventDataPacket pending_gesture_packet_;
  size_t sent_gesture_count_;
  GestureList sent_gestures_;
  gfx::Vector2dF raw_offset_;
  std::unique_ptr<GestureEventData> last_sent_gesture_;
  gfx::Rect show_press_bounding_box_;
  uint32_t last_sent_touch_event_id_;
};

TEST_F(TouchDispositionGestureFilterTest, BasicNoGestures) {
  uint32_t touch_press_event_id = PressTouchPoint().GetUniqueEventId();
  EXPECT_FALSE(GesturesSent());

  uint32_t touch_move_event_id = MoveTouchPoint().GetUniqueEventId();
  EXPECT_FALSE(GesturesSent());

  // No gestures should be dispatched by the ack, as the queued packets
  // contained no gestures.
  SendTouchConsumedAck(touch_press_event_id);
  EXPECT_FALSE(GesturesSent());

  // Release the touch gesture.
  uint32_t touch_release_event_id = ReleaseTouchPoint().GetUniqueEventId();
  SendTouchConsumedAck(touch_move_event_id);
  SendTouchConsumedAck(touch_release_event_id);
  EXPECT_FALSE(GesturesSent());
}

// On some platforms we can get a cancel per touch point. This should result
// in an empty sequence. https://crbug.com/1407442
TEST_F(TouchDispositionGestureFilterTest, ExtraCancel) {
  GestureEventDataPacket packet = GestureEventDataPacket::FromTouch(
      MockMotionEvent(MotionEvent::Action::CANCEL, base::TimeTicks(), 0, 0));
  EXPECT_EQ(TouchDispositionGestureFilter::PacketResult::EMPTY_GESTURE_SEQUENCE,
            SendGesturePacket(packet));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGestures) {
  GestureList press_gestures =
      Gestures(EventType::kGestureBegin, EventType::kGestureScrollBegin);
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), press_gestures);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(press_gestures, GetAndResetSentGestures()));

  // Multiple gestures can be queued for a single event.
  GestureList release_gestures =
      Gestures(EventType::kScrollFlingStart, EventType::kScrollFlingCancel,
               EventType::kGestureEnd);
  SendPacket(ReleaseTouchPoint(), release_gestures);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(release_gestures, GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGesturesConsumed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEnableGestureBeginEndTypes);

  // A consumed touch's gesture should only be sent on Android.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin,
                                         EventType::kGestureScrollBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureBegin),
                            GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(),
             Gestures(EventType::kScrollFlingStart,
                      EventType::kScrollFlingCancel, EventType::kGestureEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedThenNotConsumed) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch is not consumed, continue dropping gestures.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch had no consumer, continue dropping gestures.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kScrollFlingStart));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenConsumed) {
  // A not consumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  // A newly consumed gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  // And subsequent non-consumed pinch updates should not be sent.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate,
                                        EventType::kGesturePinchUpdate));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollUpdate),
                            GetAndResetSentGestures()));

  // End events dispatched only when their start events were.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollAlternatelyConsumed) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  for (size_t i = 0; i < 3; ++i) {
    SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
    SendTouchConsumedAckForLastTouch();
    EXPECT_FALSE(GesturesSent());

    SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
    SendTouchNotConsumedAckForLastTouch();
    EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollUpdate),
                              GetAndResetSentGestures()));
  }

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenNoConsumer) {
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  // If the subsequent touch has no consumer (e.g., a secondary pointer is
  // pressed but not on a touch handling rect), send the gesture.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchBegin),
                            GetAndResetSentGestures()));

  // End events should be dispatched when their start events were, independent
  // of the ack state.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchEnd),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsSent) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchBegin),
                            GetAndResetSentGestures()));

  // Consuming the touchend event can't suppress the match end gesture.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchEnd),
                            GetAndResetSentGestures()));

  // But other events in the same packet are still suppressed.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollUpdate,
                                           EventType::kGestureScrollEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));

  // EventType::kGestureScrollEnd and EventType::kScrollFlingStart behave the
  // same in this regard.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kScrollFlingStart));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kScrollFlingStart),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsNotSent) {
  // Consuming a begin event ensures no end events are sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsSuppressedPerEvent) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  // Consuming a single scroll or pinch update should suppress only that event.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchBegin),
                            GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGesturePinchUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  // Subsequent updates should not be affected.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollUpdate),
                            GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGesturePinchUpdate));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchUpdate),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchEnd),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsDependOnBeginEvents) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  // Scroll and pinch gestures depend on the scroll begin gesture being
  // dispatched.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGesturePinchUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGesturePinchEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, MultipleTouchSequences) {
  // Queue two touch-to-gestures sequences.
  uint32_t touch_press_event_id1 =
      SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  uint32_t touch_release_event_id1 =
      SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureTap));
  uint32_t touch_press_event_id2 =
      SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  uint32_t touch_release_event_id2 =
      SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollEnd));

  // The first gesture sequence should not be allowed.
  SendTouchConsumedAck(touch_press_event_id1);
  SendTouchNotConsumedAck(touch_release_event_id1);
  EXPECT_FALSE(GesturesSent());

  // The subsequent sequence should "reset" allowance.
  SendTouchNotConsumedAck(touch_press_event_id2);
  SendTouchNotConsumedAck(touch_release_event_id2);
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureScrollBegin, EventType::kGestureScrollEnd),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnNewTouchSequence) {
  const gfx::Vector2dF raw_offset(1.3f, 3.7f);
  SetRawTouchOffset(raw_offset);

  // Simulate a fling.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown,
                                         EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapCancel,
               EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kScrollFlingStart));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kScrollFlingStart),
                            GetAndResetSentGestures()));

  // A new touch sequence should cancel the outstanding fling.
  SendPacket(PressTouchPoint(1, 1), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kScrollFlingCancel),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnTouchReleaseIfNoFling) {
  // Simulate a scroll.
  // Touch position will be used for synthesized scroll end gesture.
  SendPacket(PressTouchPoint(2, 3), Gestures(EventType::kGestureTapDown,
                                             EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapCancel,
               EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnNewTouchSequence) {
  // Simulate a scroll.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown,
                                         EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapCancel,
               EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));

  // A new touch sequence should end the outstanding scroll.
  ResetTouchPoints();

  // Touch position will be used for synthesized scroll end gesture.
  SendPacket(PressTouchPoint(2, 3), NoGestures());
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnScrollBegin) {
  // Simulate a fling sequence.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown,
                                         EventType::kGestureScrollBegin,
                                         EventType::kScrollFlingStart));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapCancel,
               EventType::kGestureScrollBegin, EventType::kScrollFlingStart),
      GetAndResetSentGestures()));

  // The new fling should cancel the preceding one.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureScrollBegin,
                                           EventType::kScrollFlingStart));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kScrollFlingCancel, EventType::kGestureScrollBegin,
               EventType::kScrollFlingStart),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingNotCancelledIfGFCEventReceived) {
  // Simulate a fling that is started then cancelled.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  SendPacket(MoveTouchPoint(), Gestures(EventType::kScrollFlingStart));
  SendTouchNotConsumedAckForLastTouch();
  GestureEventDataPacket packet;
  packet.Push(CreateGesture(EventType::kScrollFlingCancel, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureScrollBegin, EventType::kScrollFlingStart,
               EventType::kScrollFlingCancel),
      GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));

  // A new touch sequence will not inject a EventType::kScrollFlingCancel, as
  // the fling has already been cancelled.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenScrollBegins) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  // If the subsequent touch turns into a scroll, the tap should be cancelled.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenTouchConsumed) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  // If the subsequent touch is consumed, the tap should be cancelled.
  GestureEventDataPacket packet;
  packet.Push(CreateGesture(EventType::kGestureScrollBegin, 2, 3, 0));
  SendTouchGestures(MoveTouchPoint(), packet);

  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

TEST_F(TouchDispositionGestureFilterTest,
       TapNotCancelledIfTapEndingEventReceived) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureTap));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureShowPress, EventType::kGestureTap),
      GetAndResetSentGestures()));

  // The tap should not be cancelled as it was terminated by a
  // |EventType::kGestureTap|.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutGestures) {
  // If the sequence is allowed, and there are no preceding gestures, the
  // timeout gestures should be forwarded immediately.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureShowPress);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureShowPress),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureLongPress);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureLongPress),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureLongTap));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureLongTap),
      GetAndResetSentGestures()));

  // If the sequence is disallowed, and there are no preceding gestures, the
  // timeout gestures should be dropped immediately.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(EventType::kGestureShowPress);
  EXPECT_FALSE(GesturesSent());
  SendPacket(ReleaseTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();

  // If the sequence has a pending ack, the timeout gestures should
  // remain queued until the ack is received.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(EventType::kGestureLongPress);
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureLongPress),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, SpuriousAcksIgnored) {
  // Acks received when the queue is empty will be safely ignored.
  ASSERT_TRUE(IsEmpty());
  SendTouchConsumedAck(0);
  EXPECT_FALSE(GesturesSent());

  uint32_t touch_press_event_id =
      SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  uint32_t touch_move_event_id =
      SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAck(touch_press_event_id);
  SendTouchNotConsumedAck(touch_move_event_id);
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureScrollBegin, EventType::kGestureScrollUpdate),
      GetAndResetSentGestures()));

  // Even if all packets have been dispatched, the filter may not be empty as
  // there could be follow-up timeout events.  Spurious acks in such cases
  // should also be safely ignored.
  ASSERT_FALSE(IsEmpty());
  SendTouchConsumedAck(0);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, PacketWithInvalidTypeIgnored) {
  GestureEventDataPacket packet;
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_TYPE,
            SendGesturePacket(packet));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, PacketsWithInvalidOrderIgnored) {
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_ORDER,
            SendTimeoutGesture(EventType::kGestureShowPress));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedTouchCancel) {
  // An unconsumed touch's gesture should be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendPacket(CancelTouchPoint(), Gestures(EventType::kGestureTapCancel,
                                          EventType::kGestureScrollEnd));
  EXPECT_FALSE(GesturesSent());
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureScrollEnd),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutEventAfterRelease) {
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureTapDown,
                                           EventType::kGestureTapUnconfirmed));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapUnconfirmed),
      GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureTap);
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureShowPress, EventType::kGestureTap),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressInsertedBeforeTap) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureTapUnconfirmed);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapUnconfirmed),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureTap));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureShowPress, EventType::kGestureTap),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressNotInsertedIfAlreadySent) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureShowPress);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureShowPress),
                            GetAndResetSentGestures()));

  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureTap));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTap),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapAndScrollCancelledOnTouchCancel) {
  const gfx::Vector2dF raw_offset(1.3f, 3.7f);
  SetRawTouchOffset(raw_offset);

  SendPacket(PressTouchPoint(1, 1), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  // A cancellation motion event should cancel the tap.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapCancel),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);

  SendPacket(PressTouchPoint(1, 1), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  // A cancellation motion event should end the scroll, even if the touch was
  // consumed.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollEnd),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
  EXPECT_EQ(LastSentGestureRawLocation(), gfx::PointF(1, 1) + raw_offset);
}

TEST_F(TouchDispositionGestureFilterTest,
       ConsumedScrollUpdateMakesFlingScrollEnd) {
  // A consumed touch's gesture should not be sent.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin,
                                         EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();

  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  GestureEventDataPacket packet;
  packet.Push(CreateGesture(EventType::kScrollFlingStart));
  packet.Push(CreateGesture(EventType::kScrollFlingCancel));
  packet.Push(CreateGesture(EventType::kGestureEnd, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);

  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureScrollEnd, EventType::kGestureEnd),
      GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin,
                                         EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureScrollBegin),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledOnTouchCancel) {
  // Touch position is used for synthesized tap cancel.
  SendPacket(PressTouchPoint(2, 3), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  // A cancellation motion event should cancel the tap.
  SendPacket(CancelTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapCancel),
                            GetAndResetSentGestures()));
  EXPECT_EQ(CurrentTouchTime(), LastSentGestureTime());
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(2, 3));
}

// Test that a GestureEvent whose dispatch causes a cancel event to be fired
// won't cause a crash.
TEST_F(TouchDispositionGestureFilterTest, TestCancelMidGesture) {
  SetCancelAfterNextGesture(true);
  // Synthesized tap cancel uses touch position.
  SendPacket(PressTouchPoint(1, 1), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapDown, EventType::kGestureTapCancel),
      GetAndResetSentGestures()));
  EXPECT_EQ(LastSentGestureLocation(), gfx::PointF(1, 1));
}

// Test that a MultiFingerSwipe event is dispatched when appropriate.
TEST_F(TouchDispositionGestureFilterTest, TestAllowedMultiFingerSwipe) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchBegin),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureSwipe));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureSwipe),
                            GetAndResetSentGestures()));
}

  // Test that a MultiFingerSwipe event is dispatched when appropriate.
TEST_F(TouchDispositionGestureFilterTest, TestDisallowedMultiFingerSwipe) {
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollBegin),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGesturePinchBegin));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGesturePinchBegin),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureSwipe));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelOnSecondFingerDown) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapCancel),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ShowPressBoundingBox) {
  GestureEventDataPacket press_packet;
  press_packet.Push(CreateGesture(EventType::kGestureTapDown, 9, 9, 8));
  SendTouchGestures(PressTouchPoint(), press_packet);

  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));

  GestureEventDataPacket release_packet;
  release_packet.Push(CreateGesture(EventType::kGestureTap, 10, 10, 10));
  SendTouchGestures(ReleaseTouchPoint(), release_packet);

  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureShowPress, EventType::kGestureTap),
      GetAndResetSentGestures()));
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), ShowPressBoundingBox());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledBeforeGestureEnd) {
  SendPacket(PressTouchPoint(),
             Gestures(EventType::kGestureBegin, EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureTapDown),
      GetAndResetSentGestures()));
  SendTimeoutGesture(EventType::kGestureShowPress);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureShowPress),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(EventType::kGestureLongPress);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureLongPress),
                            GetAndResetSentGestures()));
  SendPacket(CancelTouchPoint(), Gestures(EventType::kGestureEnd));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureEnd),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EventFlagPropagation) {
  // Real gestures should propagate flags from their causal touches.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapDown),
                            GetAndResetSentGestures()));
  EXPECT_EQ(kDefaultEventFlags, LastSentGestureFlags());

  // Synthetic gestures lack flags.
  SendPacket(PressTouchPoint(), NoGestures());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureTapCancel),
                            GetAndResetSentGestures()));
  EXPECT_EQ(0, LastSentGestureFlags());
}


TEST_F(TouchDispositionGestureFilterTest, PreviousScrollPrevented) {
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin));
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureBegin),
                            GetAndResetSentGestures()));

  // The sent scroll update should always reflect whether any preceding scroll
  // update has been dropped.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  ASSERT_TRUE(GesturesSent());
  GetAndResetSentGestures();

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchConsumedAckForLastTouch();
  EXPECT_FALSE(GesturesSent());

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  ASSERT_TRUE(GesturesSent());
  GetAndResetSentGestures();

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();
  ASSERT_TRUE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, AckQueueBack) {
  SendPacket(PressTouchPoint(1, 1), Gestures(EventType::kGestureBegin));
  SendTouchNotConsumedAckForLastTouch();

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollBegin));
  SendTouchNotConsumedAckForLastTouch();
  GetAndResetSentGestures();

  // Pending touch move.
  GestureEventDataPacket packet1;
  packet1.Push(CreateGesture(EventType::kGestureScrollUpdate, 2, 3, 0));
  uint32_t touch_event_id = SendTouchGestures(MoveTouchPoint(), packet1);
  EXPECT_FALSE(GesturesSent());

  // Additional pending touch move.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));

  // Ack back of the queue consumed.
  SendTouchConsumedAckForLastTouch();

  // Ack the pending touch.
  GetAndResetSentGestures();
  SendTouchNotConsumedAck(touch_event_id);

  // The consumed touch doesn't produce a gesture.
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureScrollUpdate),
                            GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(2, 3), LastSentGestureLocation());

  // Pending touch move.
  touch_event_id =
      SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  EXPECT_FALSE(GesturesSent());

  // Ack back of the queue unconsumed (twice).
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();

  GestureEventDataPacket packet2;
  packet2.Push(CreateGesture(EventType::kGestureScrollUpdate, 7, 8, 0));
  SendTouchGestures(MoveTouchPoint(), packet2);
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAckForLastTouch();

  // Ack the pending touch.
  GetAndResetSentGestures();
  SendTouchNotConsumedAck(touch_event_id);

  // Both touches have now been acked.
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureScrollUpdate, EventType::kGestureScrollUpdate,
               EventType::kGestureScrollUpdate),
      GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(7, 8), LastSentGestureLocation());
}

TEST_F(TouchDispositionGestureFilterTest, AckQueueGestureAtBack) {
  // Send gesture sequence
  uint32_t touch_press_event_id1 =
      SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin));
  uint32_t touch_release_event_id1 =
      SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureEnd));

  // Send second gesture sequence, and synchronously ack it.
  SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin));
  SendTouchNotConsumedAckForLastTouch();

  GestureEventDataPacket packet;
  packet.Push(CreateGesture(EventType::kGestureEnd, 2, 3, 0));
  SendTouchGestures(ReleaseTouchPoint(), packet);
  SendTouchNotConsumedAckForLastTouch();

  // The second gesture sequence is blocked on the first.
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAck(touch_press_event_id1);
  SendTouchNotConsumedAck(touch_release_event_id1);

  // Both gestures have now been acked.
  EXPECT_TRUE(
      GesturesMatch(Gestures(EventType::kGestureBegin, EventType::kGestureEnd,
                             EventType::kGestureBegin, EventType::kGestureEnd),
                    GetAndResetSentGestures()));
  EXPECT_EQ(gfx::PointF(2, 3), LastSentGestureLocation());
}

TEST_F(TouchDispositionGestureFilterTest,
       SyncAcksOnlyTriggerAppropriateGestures) {
  // Queue a touch press.
  uint32_t touch_press_event_id =
      SendPacket(PressTouchPoint(), Gestures(EventType::kGestureBegin));

  // Send and synchronously ack two touch moves.
  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();

  SendPacket(MoveTouchPoint(), Gestures(EventType::kGestureScrollUpdate));
  SendTouchNotConsumedAckForLastTouch();

  // Queue a touch release.
  uint32_t touch_release_event_id =
      SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureEnd));

  EXPECT_FALSE(GesturesSent());

  // Ack the touch press. All events but the release should be acked.
  SendTouchNotConsumedAck(touch_press_event_id);
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureScrollUpdate,
               EventType::kGestureScrollUpdate),
      GetAndResetSentGestures()));

  // The touch release still requires an ack.
  SendTouchNotConsumedAck(touch_release_event_id);
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest,
       GestureBeginEndWhenTouchStartConsumed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEnableGestureBeginEndTypes);

  // Queue a touch press, and the touch start event is consumed.
  SendPacket(PressTouchPoint(),
             Gestures(EventType::kGestureBegin, EventType::kGestureTapDown));
  SendTouchConsumedAckForLastTouch();
  // The gesture begin event is sent when the touch start is consumed.
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureBegin),
                            GetAndResetSentGestures()));

  // Queue a touch release, and the gesture end event is sent.
  SendPacket(ReleaseTouchPoint(), Gestures(EventType::kGestureEnd));
  SendTouchConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(Gestures(EventType::kGestureEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, GestureBeginEndWhenTouchEndConsumed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEnableGestureBeginEndTypes);

  // Queue a touch press.
  SendPacket(PressTouchPoint(),
             Gestures(EventType::kGestureBegin, EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureTapDown),
      GetAndResetSentGestures()));

  // Queue a touch release, and the touch end event is consumed.
  SendPacket(ReleaseTouchPoint(),
             Gestures(EventType::kGestureTap, EventType::kGestureEnd));
  SendTouchConsumedAckForLastTouch();
  // The gesture end event is sent when the touch end is consumed with the tap
  // cancel gesture event.
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureTapCancel, EventType::kGestureEnd),
      GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, GestureBeginEndWhenTouchNotConsumed) {
  // Queue a touch press.
  SendPacket(PressTouchPoint(),
             Gestures(EventType::kGestureBegin, EventType::kGestureTapDown));
  SendTouchNotConsumedAckForLastTouch();
  EXPECT_TRUE(GesturesMatch(
      Gestures(EventType::kGestureBegin, EventType::kGestureTapDown),
      GetAndResetSentGestures()));

  // Queue a touch release.
  SendPacket(ReleaseTouchPoint(),
             Gestures(EventType::kGestureTap, EventType::kGestureEnd));
  SendTouchNotConsumedAckForLastTouch();
  // The gesture end event is sent with the tap gesture event.
  EXPECT_TRUE(
      GesturesMatch(Gestures(EventType::kGestureShowPress,
                             EventType::kGestureTap, EventType::kGestureEnd),
                    GetAndResetSentGestures()));
}

}  // namespace ui
