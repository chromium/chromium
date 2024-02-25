// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/longpress_drag_selector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"

using ui::test::MockMotionEvent;

namespace ui {
namespace {

const double kSlop = 10.;

class LongPressDragSelectorTest : public testing::Test,
                                  public LongPressDragSelectorClient {
 public:
  LongPressDragSelectorTest()
      : dragging_(false), active_state_changed_(false) {}

  ~LongPressDragSelectorTest() override {}

  void SetSelection(const gfx::PointF& start, const gfx::PointF& end) {
    selection_start_ = start;
    selection_end_ = end;
  }

  bool GetAndResetActiveStateChanged() {
    bool active_state_changed = active_state_changed_;
    active_state_changed_ = false;
    return active_state_changed;
  }

  bool IsDragging() const { return dragging_; }
  const gfx::PointF& DragPosition() const { return drag_position_; }

  // LongPressDragSelectorClient implementation.
  void OnDragBegin(const TouchSelectionDraggable& handler,
                   const gfx::PointF& drag_position) override {
    dragging_ = true;
    drag_position_ = drag_position;
  }

  void OnDragUpdate(const TouchSelectionDraggable& handler,
                    const gfx::PointF& drag_position) override {
    drag_position_ = drag_position;
  }

  void OnDragEnd(const TouchSelectionDraggable& handler) override {
    dragging_ = false;
  }

  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override {
    return delta.LengthSquared() < (kSlop * kSlop);
  }

  void OnLongPressDragActiveStateChanged() override {
    active_state_changed_ = true;
  }

  gfx::PointF GetSelectionStart() const override { return selection_start_; }

  gfx::PointF GetSelectionEnd() const override { return selection_end_; }

 private:
  bool dragging_;
  bool active_state_changed_;
  gfx::PointF drag_position_;

  gfx::PointF selection_start_;
  gfx::PointF selection_end_;
};

TEST_F(LongPressDragSelectorTest, BasicDrag) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a longpress-triggered selection.
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Motion should not be consumed until a selection is detected.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Initiate drag motion.  Note that the first move event after activation is
  // used to initialize the drag start anchor.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(IsDragging());

  // The first slop exceeding motion will start the drag. As the motion is
  // downward, the end selection point should be moved.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end, DragPosition());

  // Subsequent motion will extend the selection.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end + gfx::Vector2dF(0, kSlop), DragPosition());
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 3)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end + gfx::Vector2dF(0, kSlop * 2), DragPosition());

  // Release the touch sequence, ending the drag. The selector will never
  // consume the start/end events, only move events after a longpress.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(GetAndResetActiveStateChanged());
}

TEST_F(LongPressDragSelectorTest, DoublePressDrag) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a double press triggered selection.
  constexpr gfx::PointF selection_start(0, 10);
  constexpr gfx::PointF selection_end(10, 10);
  selector.OnDoublePressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Motion should not be consumed until a selection is detected.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Initiate drag motion.  Note that the first move event after activation is
  // used to initialize the drag start anchor.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(IsDragging());

  // The first slop exceeding motion will start the drag. As the motion is
  // downward, the end selection point should be moved.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end, DragPosition());

  // Subsequent motion will extend the selection.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end + gfx::Vector2dF(0, kSlop), DragPosition());
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 3)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end + gfx::Vector2dF(0, kSlop * 2), DragPosition());

  // Release the touch sequence, ending the drag. The selector will never
  // consume the start/end events, only move events after a double press.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(GetAndResetActiveStateChanged());
}

TEST_F(LongPressDragSelectorTest, BasicReverseDrag) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a longpress-triggered selection.
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Initiate drag motion.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 5, 0)));
  EXPECT_FALSE(IsDragging());

  // As the initial motion is leftward, toward the selection start, the
  // selection start should be the drag point.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, -kSlop, 0)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_start, DragPosition());

  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, -kSlop)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_start + gfx::Vector2dF(kSlop, -kSlop), DragPosition());

  // Release the touch sequence, ending the drag.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(GetAndResetActiveStateChanged());
}

TEST_F(LongPressDragSelectorTest, NoActiveTouch) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Activate a longpress-triggered selection.
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Start a new touch sequence; it shouldn't initiate selection drag as there
  // was no active touch sequence when the longpress selection started.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(gfx::PointF(), DragPosition());
}

TEST_F(LongPressDragSelectorTest, NoLongPress) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a selection without a preceding longpress.
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Touch movement should not initiate selection drag.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(gfx::PointF(), DragPosition());
}

TEST_F(LongPressDragSelectorTest, NoValidLongPress) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  SetSelection(selection_start, selection_end);

  // Activate a longpress-triggered selection, but at a time before the current
  // touch down event.
  selector.OnLongPressEvent(event.GetEventTime() - base::Seconds(1),
                            gfx::PointF());
  selector.OnSelectionActivated();
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Activate a longpress-triggered selection, but at a place different than the
  // current touch down event.
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF(kSlop, 0));
  selector.OnSelectionActivated();
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Touch movement should not initiate selection drag.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(gfx::PointF(), DragPosition());
}

TEST_F(LongPressDragSelectorTest, NoSelection) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Trigger a longpress.
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Touch movement should not initiate selection drag, as there is no active
  // selection.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(gfx::PointF(), DragPosition());

  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
}

TEST_F(LongPressDragSelectorTest, NoDragMotion) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a longpress-triggered selection.
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Touch movement within the slop region should not initiate selection drag.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop / 2)));
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, -kSlop / 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(gfx::PointF(), DragPosition());

  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_TRUE(GetAndResetActiveStateChanged());
}

TEST_F(LongPressDragSelectorTest, SelectionDeactivated) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a longpress-triggered selection.
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Start a drag selection.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_TRUE(IsDragging());

  // Clearing the selection should force an end to the drag.
  selector.OnSelectionDeactivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Subsequent motion should not be consumed.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop)));
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.MovePoint(0, 0, kSlop * 2)));
  EXPECT_FALSE(IsDragging());
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
}

TEST_F(LongPressDragSelectorTest, DragFast) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.PressPoint(0, 0)));
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Activate a longpress-triggered selection.
  gfx::PointF selection_start(0, 10);
  gfx::PointF selection_end(10, 10);
  selector.OnLongPressEvent(event.GetEventTime(), gfx::PointF());
  EXPECT_FALSE(GetAndResetActiveStateChanged());
  SetSelection(selection_start, selection_end);
  selector.OnSelectionActivated();
  EXPECT_TRUE(GetAndResetActiveStateChanged());
  EXPECT_FALSE(IsDragging());

  // Initiate drag motion.
  EXPECT_TRUE(selector.WillHandleTouchEvent(event.MovePoint(0, 15, 5)));
  EXPECT_FALSE(IsDragging());

  // As the initial motion exceeds both endpoints, the closer bound should
  // be used for dragging, in this case the selection end.
  EXPECT_TRUE(selector.WillHandleTouchEvent(
      event.MovePoint(0, 15.f + kSlop * 2.f, 5.f + kSlop)));
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(selection_end, DragPosition());

  // Release the touch sequence, ending the drag.
  EXPECT_FALSE(selector.WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(GetAndResetActiveStateChanged());
}

TEST_F(LongPressDragSelectorTest, ScrollAfterLongPress) {
  LongPressDragSelector selector(this);
  MockMotionEvent event;
  gfx::PointF touch_point(0, 0);

  // Start a touch sequence.
  EXPECT_FALSE(selector.WillHandleTouchEvent(
      event.PressPoint(touch_point.x(), touch_point.y())));

  // Long-press and hold down.
  selector.OnLongPressEvent(event.GetEventTime(), touch_point);

  // Scroll the page. This should cancel long-press drag gesture.
  touch_point.Offset(0, 2 * kSlop);
  EXPECT_FALSE(selector.WillHandleTouchEvent(
      event.MovePoint(0, touch_point.x(), touch_point.y())));
  selector.OnScrollBeginEvent();

  // Now if the selection is activated, because long-press drag gesture was
  // canceled, active state of the long-press selector should not change.
  selector.OnSelectionActivated();
  EXPECT_FALSE(GetAndResetActiveStateChanged());

  // Release the touch sequence.
  selector.WillHandleTouchEvent(event.ReleasePoint());
}

}  // namespace
}  // namespace ui
