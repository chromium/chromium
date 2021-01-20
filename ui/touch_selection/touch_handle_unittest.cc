// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/touch_handle_orientation.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const int kDefaultTapDurationMs = 200;
const double kDefaultTapSlop = 10.;
const float kDefaultDrawableSize = 10.f;
const gfx::RectF kDefaultViewportRect(0, 0, 560, 1200);

struct MockDrawableData {
  TouchHandleOrientation orientation = TouchHandleOrientation::UNDEFINED;
  float alpha = 0.f;
  bool mirror_horizontal = false;
  bool mirror_vertical = false;
  bool enabled = false;
  bool visible = false;
  gfx::RectF rect{0, 0, kDefaultDrawableSize, kDefaultDrawableSize};
};

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(MockDrawableData* data) : data_(data) {}
  ~MockTouchHandleDrawable() override {}

  void SetEnabled(bool enabled) override { data_->enabled = enabled; }

  void SetOrientation(TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override {
    data_->orientation = orientation;
    data_->mirror_horizontal = mirror_horizontal;
    data_->mirror_vertical = mirror_vertical;
  }

  void SetOrigin(const gfx::PointF& origin) override {
    data_->rect.set_origin(origin);
  }

  void SetAlpha(float alpha) override {
    data_->alpha = alpha;
    data_->visible = alpha > 0;
  }

  // TODO(AviD): Add unittests for non-zero values of padding ratio once the
  // code refactoring is completed.
  float GetDrawableHorizontalPaddingRatio() const override { return 0; }

  gfx::RectF GetVisibleBounds() const override { return data_->rect; }

 private:
  MockDrawableData* data_;
};

}  // namespace

class TouchHandleTest : public testing::Test, public TouchHandleClient {
 public:
  TouchHandleTest()
      : dragging_(false),
        dragged_(false),
        tapped_(false),
        needs_animate_(false) {}

  ~TouchHandleTest() override {}

  // TouchHandleClient implementation.
  void OnDragBegin(const TouchSelectionDraggable& handler,
                   const gfx::PointF& drag_position) override {
    dragging_ = true;
  }

  void OnDragUpdate(const TouchSelectionDraggable& handler,
                    const gfx::PointF& drag_position) override {
    dragged_ = true;
    drag_position_ = drag_position;
  }

  void OnDragEnd(const TouchSelectionDraggable& handler) override {
    dragging_ = false;
  }

  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override {
    return delta.LengthSquared() < (kDefaultTapSlop * kDefaultTapSlop);
  }

  void OnHandleTapped(const TouchHandle& handle) override { tapped_ = true; }

  void SetNeedsAnimate() override { needs_animate_ = true; }

  std::unique_ptr<TouchHandleDrawable> CreateDrawable() override {
    return std::make_unique<MockTouchHandleDrawable>(&drawable_data_);
  }

  base::TimeDelta GetMaxTapDuration() const override {
    return base::TimeDelta::FromMilliseconds(kDefaultTapDurationMs);
  }

  bool IsAdaptiveHandleOrientationEnabled() const override {
    // Enable adaptive handle orientation by default for unittests
    return true;
  }

  void Animate(TouchHandle& handle) {
    needs_animate_ = false;
    base::TimeTicks now = base::TimeTicks::Now();
    while (handle.Animate(now))
      now += base::TimeDelta::FromMilliseconds(16);
  }

  bool GetAndResetHandleDragged() {
    bool dragged = dragged_;
    dragged_ = false;
    return dragged;
  }

  bool GetAndResetHandleTapped() {
    bool tapped = tapped_;
    tapped_ = false;
    return tapped;
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    needs_animate_ = false;
    return needs_animate;
  }

  void UpdateHandleFocus(TouchHandle& handle,
                         gfx::PointF& top,
                         gfx::PointF& bottom) {
    handle.SetFocus(top, bottom);
    handle.UpdateHandleLayout();
  }

  void UpdateHandleOrientation(TouchHandle& handle,
                               TouchHandleOrientation orientation) {
    handle.SetOrientation(orientation);
    handle.UpdateHandleLayout();
  }

  void UpdateHandleVisibility(TouchHandle& handle,
                              bool visible,
                              TouchHandle::AnimationStyle animation_style) {
    handle.SetVisible(visible, animation_style);
    handle.UpdateHandleLayout();
  }

  void UpdateViewportRect(TouchHandle& handle, gfx::RectF viewport_rect) {
    handle.SetViewportRect(viewport_rect);
    handle.UpdateHandleLayout();
  }

  bool IsDragging() const { return dragging_; }
  const gfx::PointF& DragPosition() const { return drag_position_; }
  bool NeedsAnimate() const { return needs_animate_; }

  const MockDrawableData& drawable() { return drawable_data_; }

 private:
  gfx::PointF drag_position_;
  bool dragging_;
  bool dragged_;
  bool tapped_;
  bool needs_animate_;

  MockDrawableData drawable_data_;
};

TEST_F(TouchHandleTest, Visibility) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  EXPECT_FALSE(drawable().visible);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  UpdateHandleVisibility(handle, false, TouchHandle::ANIMATION_NONE);
  EXPECT_FALSE(drawable().visible);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);
}

TEST_F(TouchHandleTest, VisibilityAnimation) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  ASSERT_FALSE(NeedsAnimate());
  ASSERT_FALSE(drawable().visible);
  ASSERT_EQ(0.f, drawable().alpha);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  Animate(handle);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  ASSERT_FALSE(NeedsAnimate());
  UpdateHandleVisibility(handle, false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  UpdateHandleVisibility(handle, false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchHandleTest, Orientation) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  EXPECT_EQ(TouchHandleOrientation::CENTER, drawable().orientation);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  UpdateHandleOrientation(handle, TouchHandleOrientation::LEFT);
  EXPECT_EQ(TouchHandleOrientation::LEFT, drawable().orientation);

  UpdateHandleOrientation(handle, TouchHandleOrientation::RIGHT);
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  UpdateHandleOrientation(handle, TouchHandleOrientation::CENTER);
  EXPECT_EQ(TouchHandleOrientation::CENTER, drawable().orientation);
}

TEST_F(TouchHandleTest, Position) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  const gfx::Vector2dF koffset_vector(kDefaultDrawableSize / 2.f, 0);
  gfx::PointF focus_top;
  gfx::PointF focus_bottom;
  EXPECT_EQ(gfx::PointF() - koffset_vector, drawable().rect.origin());

  focus_top = gfx::PointF(7.3f, -3.7f);
  focus_bottom = gfx::PointF(7.3f, -2.7f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_EQ(focus_bottom - koffset_vector, drawable().rect.origin());

  focus_top = gfx::PointF(-7.3f, 3.7f);
  focus_bottom = gfx::PointF(-7.3f, 4.7f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_EQ(focus_bottom - koffset_vector, drawable().rect.origin());
}

TEST_F(TouchHandleTest, PositionNotUpdatedWhileFadingOrInvisible) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  ASSERT_TRUE(drawable().visible);
  ASSERT_FALSE(NeedsAnimate());

  const gfx::Vector2dF koffset_vector(kDefaultDrawableSize / 2.f, 0);
  gfx::PointF old_focus_top(7.3f, -3.7f);
  gfx::PointF old_focus_bottom(7.3f, -2.7f);
  UpdateHandleFocus(handle, old_focus_top, old_focus_bottom);
  ASSERT_EQ(old_focus_bottom - koffset_vector, drawable().rect.origin());

  UpdateHandleVisibility(handle, false, TouchHandle::ANIMATION_SMOOTH);
  ASSERT_TRUE(NeedsAnimate());

  gfx::PointF new_position_top(3.7f, -3.7f);
  gfx::PointF new_position_bottom(3.7f, -2.7f);
  UpdateHandleFocus(handle, new_position_top, new_position_bottom);
  EXPECT_EQ(old_focus_bottom - koffset_vector, drawable().rect.origin());
  EXPECT_TRUE(NeedsAnimate());

  // While the handle is fading, the new position should not take affect.
  base::TimeTicks now = base::TimeTicks::Now();
  while (handle.Animate(now)) {
    EXPECT_EQ(old_focus_bottom - koffset_vector, drawable().rect.origin());
    now += base::TimeDelta::FromMilliseconds(16);
  }

  // Even after the animation terminates, the new position will not be pushed.
  EXPECT_EQ(old_focus_bottom - koffset_vector, drawable().rect.origin());

  // As soon as the handle becomes visible, the new position will be pushed.
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(new_position_bottom - koffset_vector, drawable().rect.origin());
}

TEST_F(TouchHandleTest, Enabled) {
  // A newly created handle defaults to enabled.
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  EXPECT_TRUE(drawable().enabled);

  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  EXPECT_EQ(0.f, drawable().alpha);
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);

  // Dragging should not be allowed while the handle is disabled.
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, kOffset,
                        kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));

  // Disabling mid-animation should cancel the animation.
  handle.SetEnabled(true);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(drawable().enabled);
  EXPECT_EQ(0.f, drawable().alpha);
  // Since alpha value is 0, visibility of drawable will be false.
  EXPECT_FALSE(drawable().visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);
  EXPECT_FALSE(drawable().visible);
  EXPECT_FALSE(handle.Animate(base::TimeTicks::Now()));

  // Disabling mid-drag should cancel the drag.
  handle.SetEnabled(true);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());
  handle.SetEnabled(false);
  EXPECT_FALSE(IsDragging());
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, Drag) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;

  // The handle must be visible to trigger drag.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, kOffset,
                        kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  // Action::DOWN must fall within the drawable region to trigger drag.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 50, 50);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Only Action::DOWN will trigger drag.
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, kOffset,
                          kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Start the drag.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, kOffset,
                          kOffset);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          kOffset + 10, kOffset + 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(10, 15), DragPosition());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          kOffset - 10, kOffset - 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(-10, -15), DragPosition());

  event = MockMotionEvent(MockMotionEvent::Action::UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());

  // Non-Action::DOWN events after the drag has terminated should not be
  // handled.
  event = MockMotionEvent(MockMotionEvent::Action::CANCEL);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, DragDefersOrientationChange) {
  TouchHandle handle(this, TouchHandleOrientation::RIGHT, kDefaultViewportRect);
  ASSERT_EQ(drawable().orientation, TouchHandleOrientation::RIGHT);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::Action::DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Orientation changes will be deferred until the drag ends.
  UpdateHandleOrientation(handle, TouchHandleOrientation::LEFT);
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::Action::MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(TouchHandleOrientation::RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::Action::UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(TouchHandleOrientation::LEFT, drawable().orientation);
}

TEST_F(TouchHandleTest, DragDefersFade) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::Action::DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Fade will be deferred until the drag ends.
  UpdateHandleVisibility(handle, false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  event = MockMotionEvent(MockMotionEvent::Action::MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);

  event = MockMotionEvent(MockMotionEvent::Action::UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(NeedsAnimate());

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);
}

TEST_F(TouchHandleTest, DragTargettingUsesTouchSize) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  gfx::PointF focus_top(kDefaultDrawableSize / 2, 0);
  gfx::PointF focus_bottom(kDefaultDrawableSize / 2, 0);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kTouchSize = 24.f;
  const float kOffset = kDefaultDrawableSize + kTouchSize / 2.001f;

  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, kOffset, 0);
  event.SetTouchMajor(0.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize / 2.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event.SetTouchMajor(kTouchSize * 2.f);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // The touch hit test region should be circular.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, kOffset,
                          kOffset);
  event.SetTouchMajor(kTouchSize);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize * std::sqrt(2.f) - 0.1f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize * std::sqrt(2.f) + 0.1f);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Ensure a touch size of 0 can still register a hit.
  event =
      MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                      kDefaultDrawableSize / 2.f, kDefaultDrawableSize / 2.f);
  event.SetTouchMajor(0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Touches centered above the handle region should never register a hit, even
  // if the touch region intersects the handle region.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          kDefaultDrawableSize / 2.f, -kTouchSize / 3.f);
  event.SetTouchMajor(kTouchSize);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
}

TEST_F(TouchHandleTest, Tap) {
  TouchHandle handle(this, TouchHandleOrientation::CENTER,
                     kDefaultViewportRect);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();

  // Action::CANCEL shouldn't trigger a tap.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += base::TimeDelta::FromMilliseconds(50);
  event = MockMotionEvent(MockMotionEvent::Action::CANCEL, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Long press shouldn't trigger a tap.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += 2 * GetMaxTapDuration();
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Only a brief tap within the slop region should trigger a tap.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += GetMaxTapDuration() / 2;
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          kDefaultTapSlop / 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          kDefaultTapSlop / 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleTapped());

  // Moving beyond the slop region shouldn't trigger a tap.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += GetMaxTapDuration() / 2;
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          kDefaultTapSlop * 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          kDefaultTapSlop * 2.f, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());
}

TEST_F(TouchHandleTest, MirrorFocusChange) {
  TouchHandle handle(this, TouchHandleOrientation::LEFT, kDefaultViewportRect);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  gfx::PointF focus_top;
  gfx::PointF focus_bottom;
  EXPECT_EQ(gfx::PointF(), drawable().rect.origin());

  // Moving the selection to the bottom of the screen
  // should mirror the handle vertically.
  focus_top = gfx::PointF(17.3f, 1199.0f);
  focus_bottom = gfx::PointF(17.3f, 1200.0f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_TRUE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);

  // Moving the left handle to the left edge of the viewport
  // should mirror the handle horizontally as well.
  focus_top = gfx::PointF(2.3f, 1199.0f);
  focus_bottom = gfx::PointF(2.3f, 1200.0f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_TRUE(drawable().mirror_vertical);
  EXPECT_TRUE(drawable().mirror_horizontal);

  // When the selection is not at the bottom, only the
  // horizontal mirror flag should be true.
  focus_top = gfx::PointF(2.3f, 7.3f);
  focus_bottom = gfx::PointF(2.3f, 8.3f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_TRUE(drawable().mirror_horizontal);

  // When selection handles intersects the viewport fully,
  // both mirror values should be false.
  focus_top = gfx::PointF(23.3f, 7.3f);
  focus_bottom = gfx::PointF(23.3f, 8.3f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);

  // Horizontal mirror should be true for Right handle when
  // the handle is at theright edge of the viewport.
  UpdateHandleOrientation(handle, TouchHandleOrientation::RIGHT);
  focus_top = gfx::PointF(560.0f, 7.3f);
  focus_bottom = gfx::PointF(560.0f, 8.3f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_TRUE(drawable().mirror_horizontal);
}

TEST_F(TouchHandleTest, DragDefersMirrorChange) {
  TouchHandle handle(this, TouchHandleOrientation::RIGHT, kDefaultViewportRect);
  ASSERT_EQ(drawable().orientation, TouchHandleOrientation::RIGHT);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;

  // Start the drag.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, kOffset,
                        kOffset);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  UpdateHandleOrientation(handle, TouchHandleOrientation::LEFT);
  gfx::PointF focus_top(17.3f, 1199.0f);
  gfx::PointF focus_bottom(17.3f, 1200.0f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);

  // Mirror flag changes will be deferred until the drag ends.
  event = MockMotionEvent(MockMotionEvent::Action::UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);
}

TEST_F(TouchHandleTest, ViewportSizeChange) {
  TouchHandle handle(this, TouchHandleOrientation::RIGHT, kDefaultViewportRect);
  ASSERT_EQ(drawable().orientation, TouchHandleOrientation::RIGHT);
  UpdateHandleVisibility(handle, true, TouchHandle::ANIMATION_NONE);

  gfx::PointF focus_top;
  gfx::PointF focus_bottom;
  EXPECT_EQ(gfx::PointF(), drawable().rect.origin());

  focus_top = gfx::PointF(230.0f, 599.0f);
  focus_bottom = gfx::PointF(230.0f, 600.0f);
  UpdateHandleFocus(handle, focus_top, focus_bottom);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);

  UpdateViewportRect(handle, gfx::RectF(0, 0, 560, 600));
  EXPECT_TRUE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);

  UpdateViewportRect(handle, gfx::RectF(0, 0, 230, 600));
  EXPECT_TRUE(drawable().mirror_vertical);
  EXPECT_TRUE(drawable().mirror_horizontal);

  UpdateViewportRect(handle, kDefaultViewportRect);
  EXPECT_FALSE(drawable().mirror_vertical);
  EXPECT_FALSE(drawable().mirror_horizontal);
}

}  // namespace ui
