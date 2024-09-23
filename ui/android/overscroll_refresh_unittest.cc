// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh.h"
#include "base/android/scoped_java_ref.h"
#include "cc/input/overscroll_behavior.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/gfx/geometry/point_f.h"

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

  void PullUpdate(float x_delta, float y_delta) override { delta_ += y_delta; }

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

  float GetAndResetPullDelta() {
    float result = delta_;
    delta_ = 0;
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
    effect.OnOverscrolled(ob);
    EXPECT_EQ(started, GetAndResetPullStarted());
    EXPECT_EQ(!started, GetAndResetPullReset());
  }

 private:
  float delta_ = 0;
  bool started_ = false;
  bool released_ = false;
  bool reset_ = false;
  bool refresh_allowed_ = false;
};

TEST_F(OverscrollRefreshTest, TriggerPullToRefresh) {
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
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_TRUE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDelta());
  EXPECT_TRUE(effect.IsActive());

  // Even scrolls in the down direction should be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDelta());
  EXPECT_TRUE(effect.IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetPullReleased());
  effect.OnScrollEnd(zero_velocity);
  EXPECT_FALSE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_TRUE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialYOffsetIsNotZero) {
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
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfOverflowYHidden) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);

  // overflow-y:hidden at the start of scroll will prevent activation.
  gfx::PointF zero_offset;
  bool overflow_y_hidden = true;
  gfx::SizeF viewport(100, 100);
  gfx::SizeF content_size(100, 10000);
  effect.OnFrameUpdated(viewport, zero_offset, content_size, overflow_y_hidden);
  effect.OnScrollBegin(kStartPos);

  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollDownward) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);

  // A downward initial scroll will prevent activation, even if the subsequent
  // scroll overscrolls upward.
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -10)));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());

  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollOrTouchConsumed) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());

  // Consumption of the initial touchmove or scroll should prevent future
  // activation.
  effect.Reset();
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetPullStarted());
  EXPECT_FALSE(GetAndResetPullReleased());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfFlungDownward) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
  ASSERT_TRUE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Terminating the pull with a down-directed fling should prevent triggering.
  effect.OnScrollEnd(gfx::Vector2dF(0, -1000));
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfReleasedWithoutActivation) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
  ASSERT_TRUE(effect.IsActive());
  EXPECT_TRUE(GetAndResetPullStarted());

  // An early release should prevent the refresh action from firing.
  effect.ReleaseWithoutActivation();
  effect.OnScrollEnd(gfx::Vector2dF());
  EXPECT_TRUE(GetAndResetPullReleased());
  EXPECT_FALSE(GetAndResetRefreshAllowed());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfReset) {
  OverscrollRefresh effect(this, kDefaultEdgeWidth);
  effect.OnScrollBegin(kStartPos);
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
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
  effect.OnOverscrolled(cc::OverscrollBehavior());
  EXPECT_TRUE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  EXPECT_TRUE(GetAndResetPullStarted());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_EQ(-50.f, GetAndResetPullDelta());
  EXPECT_TRUE(effect.IsActive());

  // Even scrolls in the different direction should be consumed.
  EXPECT_TRUE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_EQ(50.f, GetAndResetPullDelta());
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
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -10)));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
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
  ASSERT_FALSE(effect.WillHandleScrollUpdate(gfx::Vector2dF(0, -10)));
  EXPECT_FALSE(effect.IsActive());
  EXPECT_FALSE(effect.IsAwaitingScrollUpdateAck());
  effect.OnOverscrolled(cc::OverscrollBehavior());
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
