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
const gfx::PointF kStartPos(2.f, 2.f);
const float kDefaultEdgeWidth =
    OverscrollRefresh::kDefaultNavigationEdgeWidth * kDipScale;

class OverscrollRefreshTest : public OverscrollRefreshHandler,
                              public testing::Test {
 public:
  OverscrollRefreshTest() : OverscrollRefreshHandler(nullptr) {}

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
    OverscrollRefresh effect(this, kDefaultEdgeWidth);
    effect.OnScrollBegin(kStartPos);
    EXPECT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
    EXPECT_FALSE(effect.IsActive());
    EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());
    effect.OnOverscrolled(ob, -scroll_delta,
                          blink::WebGestureDevice::kTouchscreen);
    EXPECT_EQ(started, GetAndResetPullStarted());
    EXPECT_EQ(!started, GetAndResetPullReset());
  }

 private:
  float x_delta_ = 0;
  float y_delta_ = 0;
  bool started_ = false;
  bool released_ = false;
  bool reset_ = false;
  bool refresh_allowed_ = false;
};

TEST_F(OverscrollRefreshTest, TriggerPullToRefreshWithTouchscreen) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());

  effect.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_up(0, 10);
  EXPECT_FALSE(effect.WillHandleScrollUpdate(scroll_up));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_up,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect.IsActive());

  // Even scrolls in the down direction should be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredWithTouchpad) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());

  effect.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_up(0, 10);
  EXPECT_FALSE(effect.WillHandleScrollUpdate(scroll_up));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll from a touchpad will not trigger the
  // effect.
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_up,
                        blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(GetAndResetPullStarted());

  // Further scrolls will not be consumed.
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfInitialYOffsetIsNotZero) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // A positive y scroll offset at the start of scroll will prevent activation,
  // even if the subsequent scroll overscrolls upward.
  gfx::PointF nonzero_offset(0, 10);
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  bool overflow_y_hidden = false;
  effect.OnFrameUpdated(viewport, nonzero_offset, content_size,
                        overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size,
                        overflow_y_hidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfOverflowYHidden) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // overflow-y:hidden at the start of scroll will prevent activation.
  gfx::PointF zero_offset;
  bool overflow_y_hidden = true;
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  effect.OnFrameUpdated(viewport, zero_offset, content_size, overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest,
       RefreshNotTriggeredIfOverflowYHiddenNoUpdateBeforeOverscroll) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // overflow-y:hidden at the start of scroll will prevent activation.
  gfx::PointF zero_offset;
  bool overflow_y_hidden = true;
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  effect.OnFrameUpdated(viewport, zero_offset, content_size, overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfInitialScrollDownward) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);

  // A downward initial scroll will prevent activation, even if the subsequent
  // scroll overscrolls upward.
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());

  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest,
       RefreshNotTriggeredIfInitialScrollOrTouchConsumed) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // Consumption of the initial touchmove or scroll should prevent future
  // activation.
  effect.Reset();
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfFlungDownward) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Terminating the pull with a down-directed fling should prevent triggering.
  effect.OnScrollEnd(gfx::Vector2dF(0, -1000));
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfReleasedWithoutActivation) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // An early release should prevent the refresh action from firing.
  effect.ReleaseWithoutActivation();
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, RefreshNotTriggeredIfReset) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  ASSERT_TRUE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // An early reset should prevent the refresh action from firing.
  effect.Reset();
  EXPECT_TRUE(GetAndResetPullReset());
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, TriggerPullFromBottomEdge) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // Set yOffset as reaching the bottom of the page.
  gfx::PointF nonzero_offset(0, 900);
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 1000);
  bool overflow_y_hidden = false;
  effect.OnFrameUpdated(viewport, nonzero_offset, content_size,
                        overflow_y_hidden);

  gfx::PointF start(2.f, 902.f);
  effect.OnScrollBegin(start);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_down(0, -10);
  EXPECT_FALSE(effect.WillHandleScrollUpdate(scroll_down));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_down,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect.IsActive());

  // Even scrolls in the different direction should be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaY());
  EXPECT_TRUE(effect.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollNotFromBottom) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // A negative y scroll offset at the start of scroll will prevent activation,
  // since it's not starting from the bottom, even if the subsequent scroll
  // overscrolls upward.
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 110);
  bool overflow_y_hidden = false;
  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size,
                        overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size,
                        overflow_y_hidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfContentSizeEqualsToViewport) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // bottom overscroll only triggers when content is scrollable.
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 100);
  bool overflow_y_hidden = false;
  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size,
                        overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size,
                        overflow_y_hidden);
  gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, -10);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(scroll_delta));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_delta,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -500)));
  effect.OnScrollEnd(gfx::Vector2dF());
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
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // Make sure viewport width is larger than edge width * 2.
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size, true);

  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());

  effect.OnScrollBegin(kStartPos);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_right(10, 00);
  EXPECT_FALSE(effect.WillHandleScrollUpdate(scroll_right));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect.
  effect.OnOverscrolled(cc::OverscrollBehavior(), -scroll_right,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further right scrolls will be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(50, 0)));
  EXPECT_EQ(50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect.IsActive());

  // Even scrolls in the left direction should be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(-50, 0)));
  EXPECT_EQ(-50.f, GetAndResetPullDeltaX());
  EXPECT_TRUE(effect.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NavigateNotTriggeredIfStartAwayFromEdge) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // Make sure viewport width is larger than edge width * 2.
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  effect.OnFrameUpdated(viewport, gfx::PointF(), content_size, true);

  // Start from position with x-coordinate towards center by edge width.
  auto startPos = kStartPos + gfx::Vector2dF(kDefaultEdgeWidth, 0);
  effect.OnScrollBegin(startPos);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(effect.IsAwaitingScrollUpdateAck());

  gfx::Vector2dF scroll_right = gfx::Vector2dF(10, 0);
  effect.OnOverscrolled(cc::OverscrollBehavior(), scroll_right,
                        blink::WebGestureDevice::kTouchscreen);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(500, 0)));
  effect.OnScrollEnd(gfx::Vector2dF());
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
