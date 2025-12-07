// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh.h"

#include "base/android/scoped_java_ref.h"
#include "cc/input/overscroll_behavior.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

const float kDipScale = 1.f;
const float kDefaultEdgeWidth =
    OverscrollRefresh::kDefaultNavigationEdgeWidth * kDipScale;
const gfx::SizeF kViewport(100, 100);
const gfx::PointF kZeroOffset(0, 0);
const gfx::SizeF kContentSize(100, 10000);
const bool kOverflowYNotHidden = false;
const gfx::PointF kStartPos(2.f, 2.f);
const gfx::PointF kStartPosNotEdge = gfx::PointF(2.f + kDefaultEdgeWidth, 2.f);

class OverscrollRefreshTest : public OverscrollRefreshHandler,
                              public testing::Test {
 public:
  OverscrollRefreshTest()
      : OverscrollRefreshHandler(nullptr),
        effect_(OverscrollRefresh(this, kDefaultEdgeWidth)) {
    effect_.OnFrameUpdated(kViewport, kZeroOffset, kContentSize,
                           kOverflowYNotHidden);
  }

  // OverscrollRefreshHandler implementation.
  bool PullStart(
      OverscrollAction type,
      std::optional<BackGestureEventSwipeEdge> initiating_edge) override {
    started_ = true;
    return true;
  }

  void PullUpdate(float x_delta, float y_delta) override {
    x_delta_ += x_delta;
    y_delta_ += y_delta;
  }

  void PullRelease(bool allow_refresh) override {
    released_ = true;
    refresh_allowed_ = allow_refresh;
  }

  void PullReset() override { reset_ = true; }

  bool GetAndResetPullStarted() {
    bool result = started_;
    started_ = false;
    return result;
  }

  float GetAndResetPullDeltaX() {
    float result = x_delta_;
    x_delta_ = 0;
    y_delta_ = 0;
    return result;
  }

  float GetAndResetPullDeltaY() {
    float result = y_delta_;
    x_delta_ = 0;
    y_delta_ = 0;
    return result;
  }

  bool GetAndResetPullReleased() {
    bool result = released_;
    released_ = false;
    return result;
  }

  bool GetAndResetRefreshAllowed() {
    bool result = refresh_allowed_;
    refresh_allowed_ = false;
    return result;
  }

  bool GetAndResetPullReset() {
    bool result = reset_;
    reset_ = false;
    return result;
  }

  void TestOverscrollBehavior(const cc::OverscrollBehavior& ob,
                              const gfx::Vector2dF& scroll_delta,
                              bool started) {
    effect_.OnScrollBegin(kStartPos);
    EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
    EXPECT_FALSE(effect_.IsActive());
    EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());
    effect_.OnOverscrolled(ob, -scroll_delta,
                           blink::WebGestureDevice::kTouchscreen);
    EXPECT_EQ(started, GetAndResetPullStarted());
    EXPECT_EQ(!started, GetAndResetPullReset());
  }

  OverscrollRefresh effect_;

 private:
  float x_delta_ = 0;
  float y_delta_ = 0;
  bool started_ = false;
  bool released_ = false;
  bool reset_ = false;
  bool refresh_allowed_ = false;
};

TEST_F(OverscrollRefreshTest, TriggerPullToRefreshWithTouchscreen) {
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());

  effect_.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_up(0, 10);
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_up));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_up,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect_.IsActive());

  // Even scrolls in the down direction should be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect_.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect_.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredWithTouchpad) {
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());

  effect_.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_up(0, 10);
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_up));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll from a touchpad will not trigger the
  // effect.
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_up,
                         blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(GetAndResetPullStarted());

  // Further scrolls will not be consumed.
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfInitialYOffsetIsNotZero) {
  // A positive y scroll offset at the start of scroll will prevent activation,
  // even if the subsequent scroll overscrolls upward.
  gfx::PointF nonzero_offset(0, 10);
  effect_.OnFrameUpdated(kViewport, nonzero_offset, kContentSize,
                         kOverflowYNotHidden);
  effect_.OnScrollBegin(kStartPos);

  effect_.OnFrameUpdated(kViewport, kZeroOffset, kContentSize,
                         kOverflowYNotHidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfOverflowYHidden) {
  // overflow-y:hidden at the start of scroll will prevent activation.
  effect_.OnFrameUpdated(kViewport, kZeroOffset, kContentSize, true);
  effect_.OnScrollBegin(kStartPos);

  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest,
       RefreshNotTriggeredIfOverflowYHiddenNoUpdateBeforeOverscroll) {
  // overflow-y:hidden at the start of scroll will prevent activation.
  effect_.OnFrameUpdated(kViewport, kZeroOffset, kContentSize, true);
  effect_.OnScrollBegin(kStartPos);

  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfInitialScrollDownward) {
  effect_.OnScrollBegin(kStartPos);

  // A downward initial scroll will prevent activation, even if the subsequent
  // scroll overscrolls upward.
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());

  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest,
       RefreshNotTriggeredIfInitialScrollOrTouchConsumed) {
  effect_.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // Consumption of the initial touchmove or scroll should prevent future
  // activation.
  effect_.Reset();
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfFlungDownward) {
  effect_.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Terminating the pull with a down-directed fling should prevent triggering.
  effect_.OnScrollEnd(gfx::Vector2dF(0, -1000));
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfReleasedWithoutActivation) {
  effect_.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // An early release should prevent the refresh action from firing.
  effect_.ReleaseWithoutActivation();
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfReset) {
  effect_.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // An early reset should prevent the refresh action from firing.
  effect_.Reset();
  EXPECT_TRUE(GetAndResetPullReset());
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, TriggerPullFromBottomEdge) {
  // Set yOffset as reaching the bottom of the page.
  gfx::PointF nonzero_offset(0, 900);
  gfx::SizeF content_size(100, 1000);
  effect_.OnFrameUpdated(kViewport, nonzero_offset, content_size,
                         kOverflowYNotHidden);

  gfx::PointF start(2.f, 902.f);
  effect_.OnScrollBegin(start);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_down(0, -10);
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_down));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_down,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect_.IsActive());

  // Even scrolls in the different direction should be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect_.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect_.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollNotFromBottom) {
  // A negative y scroll offset at the start of scroll will prevent activation,
  // since it's not starting from the bottom, even if the subsequent scroll
  // overscrolls upward.
  gfx::SizeF content_size(100, 110);
  effect_.OnFrameUpdated(kViewport, kZeroOffset, content_size,
                         kOverflowYNotHidden);
  effect_.OnScrollBegin(kStartPos);

  effect_.OnFrameUpdated(kViewport, kZeroOffset, content_size,
                         kOverflowYNotHidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, -500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfContentSizeEqualsToViewport) {
  // bottom overscroll only triggers when content is scrollable.
  effect_.OnFrameUpdated(kViewport, gfx::PointF(), kViewport,
                         kOverflowYNotHidden);
  effect_.OnScrollBegin(kStartPos);

  effect_.OnFrameUpdated(kViewport, gfx::PointF(), kViewport,
                         kOverflowYNotHidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect_.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(0, -500)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorYAutoTriggersStart) {
  TestOverscrollBehavior(cc::OverscrollBehavior(), gfx::Vector2dF(0, 10), true);
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorYContainPreventsTriggerStart) {
  auto ob = cc::OverscrollBehavior();
  ob.y = cc::OverscrollBehavior::Type::kContain;
  TestOverscrollBehavior(ob, gfx::Vector2dF(0, 10), false);
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorYNonePreventsTriggerStart) {
  auto ob = cc::OverscrollBehavior();
  ob.y = cc::OverscrollBehavior::Type::kNone;
  TestOverscrollBehavior(ob, gfx::Vector2dF(0, 10), false);
}

TEST_F(OverscrollRefreshTest, TriggerSwipeToNavigate) {
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());

  effect_.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_right(10, 00);
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_right));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_right,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further right scrolls will be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(50, 0)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect_.IsActive());

  // Even scrolls in the left direction should be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(-50, 0)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect_.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect_.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest,
       NavigateNotTriggeredIfStartAwayFromEdgeOnTouchscreen) {
  // Start from position with x-coordinate towards center by edge width.
  effect_.OnScrollBegin(kStartPosNotEdge);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  gfx::Vector2dF scroll_right = gfx::Vector2dF(10, 0);
  effect_.OnOverscrolled(cc::OverscrollBehavior(), scroll_right,
                         blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(500, 0)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest,
       TriggerSwipeToNavigateEverywhereOnTouchpadWithFeatureEnabled) {
  effect_.SetTouchpadOverscrollHistoryNavigation(true);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());

  // Start from position with x-coordinate towards center by edge width.
  effect_.OnScrollBegin(kStartPosNotEdge);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_right(10, 00);
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(scroll_right));
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect_.OnOverscrolled(cc::OverscrollBehavior(), -scroll_right,
                         blink::WebGestureDevice::kTouchpad);
  EXPECT_TRUE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further right scrolls will be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(50, 0)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect_.IsActive());

  // Even scrolls in the left direction should be consumed.
  EXPECT_TRUE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(-50, 0)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect_.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect_.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest,
       NavigateNotTriggeredIfStartAwayFromEdgeOnTouchpadWithFeatureDisabled) {
  // Feature is disabled by default on construction.
  // Start from position with x-coordinate towards center by edge width.
  effect_.OnScrollBegin(kStartPosNotEdge);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_TRUE(effect_.IsAwaitingScrollUpdateAck());

  gfx::Vector2dF scroll_right = gfx::Vector2dF(10, 0);
  effect_.OnOverscrolled(cc::OverscrollBehavior(), scroll_right,
                         blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(effect_.IsActive());
  EXPECT_FALSE(effect_.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect_.WillHandleScrollUpdate(gfx::Vector2dF(500, 0)));
  effect_.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorXAutoTriggersStart) {
  TestOverscrollBehavior(cc::OverscrollBehavior(), gfx::Vector2dF(10, 0), true);
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorXContainPreventsTriggerStart) {
  auto ob = cc::OverscrollBehavior();
  ob.x = cc::OverscrollBehavior::Type::kContain;
  TestOverscrollBehavior(ob, gfx::Vector2dF(10, 0), false);
}

TEST_F(OverscrollRefreshTest, OverscrollBehaviorXNonePreventsTriggerStart) {
  auto ob = cc::OverscrollBehavior();
  ob.x = cc::OverscrollBehavior::Type::kNone;
  TestOverscrollBehavior(ob, gfx::Vector2dF(10, 0), false);
}

}  // namespace ui
