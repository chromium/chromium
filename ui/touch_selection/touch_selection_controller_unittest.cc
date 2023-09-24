// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_controller.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/touch_selection/touch_selection_controller_test_api.h"

using testing::ElementsAre;
using testing::IsEmpty;
using ui::test::MockMotionEvent;

namespace ui {
namespace {

constexpr int kDefaultTapTimeoutMs = 200;
constexpr float kDefaultTapSlop = 10.f;
constexpr gfx::PointF kIgnoredPoint(0, 0);

constexpr TouchSelectionController::Config kDefaultConfig = {
    .max_tap_duration = base::Milliseconds(kDefaultTapTimeoutMs),
    .tap_slop = kDefaultTapSlop,
};

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(bool* contains_point)
      : intersects_rect_(contains_point) {}

  MockTouchHandleDrawable(const MockTouchHandleDrawable&) = delete;
  MockTouchHandleDrawable& operator=(const MockTouchHandleDrawable&) = delete;

  ~MockTouchHandleDrawable() override {}
  void SetEnabled(bool enabled) override {}
  void SetOrientation(TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override {}
  void SetOrigin(const gfx::PointF& origin) override {}
  void SetAlpha(float alpha) override {}
  gfx::RectF GetVisibleBounds() const override {
    return *intersects_rect_ ? gfx::RectF(-1000, -1000, 2000, 2000)
                             : gfx::RectF(-1000, -1000, 0, 0);
  }
  float GetDrawableHorizontalPaddingRatio() const override { return 0; }

 private:
  raw_ptr<bool> intersects_rect_;
};

class TouchSelectionControllerTest : public testing::Test,
                                     public TouchSelectionControllerClient {
 public:
  TouchSelectionControllerTest() = default;

  TouchSelectionControllerTest(const TouchSelectionControllerTest&) = delete;
  TouchSelectionControllerTest& operator=(const TouchSelectionControllerTest&) =
      delete;

  ~TouchSelectionControllerTest() override {}

  // testing::Test implementation.

  void SetUp() override {
    InitializeControllerWithConfig(kDefaultConfig);
    StartTouchEventSequence();
  }

  void TearDown() override { controller_.reset(); }

  // TouchSelectionControllerClient implementation.

  bool SupportsAnimation() const override { return animation_enabled_; }

  void SetNeedsAnimate() override { needs_animate_ = true; }

  void MoveCaret(const gfx::PointF& position) override {
    caret_moved_ = true;
    caret_position_ = position;
  }

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override {
    if (base == selection_end_ && extent == selection_start_)
      selection_points_swapped_ = true;

    selection_start_ = base;
    selection_end_ = extent;
  }

  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {
    selection_moved_ = true;
    selection_end_ = extent;
  }

  void OnSelectionEvent(SelectionEventType event) override {
    events_.push_back(event);
    last_event_start_ = controller_->GetStartPosition();
    last_event_end_ = controller_->GetEndPosition();
    last_event_bounds_rect_ = controller_->GetRectBetweenBounds();
  }

  void OnDragUpdate(const TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override {
    last_drag_update_position_ = position;
  }

  std::unique_ptr<TouchHandleDrawable> CreateDrawable() override {
    return std::make_unique<MockTouchHandleDrawable>(&dragging_enabled_);
  }

  void DidScroll() override {}

  void InitializeControllerWithConfig(TouchSelectionController::Config config) {
    controller_ = std::make_unique<TouchSelectionController>(this, config);
  }

  void StartTouchEventSequence() {
    controller_->WillHandleTouchEvent(
        MockMotionEvent(MotionEvent::Action::DOWN));
  }

  void SetAnimationEnabled(bool enabled) { animation_enabled_ = enabled; }
  void SetDraggingEnabled(bool enabled) { dragging_enabled_ = enabled; }

  void ClearSelection() {
    controller_->OnSelectionBoundsChanged(gfx::SelectionBound(),
                                          gfx::SelectionBound());
  }

  void ClearInsertion() { ClearSelection(); }

  void ChangeInsertion(const gfx::RectF& rect, bool visible) {
    gfx::SelectionBound bound;
    bound.set_type(gfx::SelectionBound::CENTER);
    bound.SetEdge(rect.origin(), rect.bottom_left());
    bound.set_visible(visible);
    controller_->OnSelectionBoundsChanged(bound, bound);
  }

  void ChangeSelection(const gfx::RectF& start_rect,
                       bool start_visible,
                       const gfx::RectF& end_rect,
                       bool end_visible) {
    gfx::SelectionBound start_bound, end_bound;
    start_bound.set_type(gfx::SelectionBound::LEFT);
    end_bound.set_type(gfx::SelectionBound::RIGHT);
    start_bound.SetEdge(start_rect.origin(), start_rect.bottom_left());
    end_bound.SetEdge(end_rect.origin(), end_rect.bottom_left());
    start_bound.set_visible(start_visible);
    end_bound.set_visible(end_visible);
    controller_->OnSelectionBoundsChanged(start_bound, end_bound);
  }

  void ChangeVerticalSelection(const gfx::RectF& start_rect,
                               bool start_visible,
                               const gfx::RectF& end_rect,
                               bool end_visible) {
    gfx::SelectionBound start_bound, end_bound;
    start_bound.set_type(gfx::SelectionBound::RIGHT);
    end_bound.set_type(gfx::SelectionBound::LEFT);
    start_bound.SetEdge(start_rect.origin(), start_rect.bottom_right());
    end_bound.SetEdge(end_rect.bottom_right(), end_rect.origin());
    start_bound.set_visible(start_visible);
    end_bound.set_visible(end_visible);
    controller_->OnSelectionBoundsChanged(start_bound, end_bound);
  }

  void OnLongPressEvent() {
    controller().HandleLongPressEvent(base::TimeTicks(),
                                          kIgnoredPoint);
  }

  void OnDoublePressEvent() {
    controller().HandleDoublePressEvent(base::TimeTicks(), kIgnoredPoint);
  }

  void OnTapEvent() {
    controller().HandleTapEvent(kIgnoredPoint, 1);
  }

  void OnDoubleTapEvent() {
    controller().HandleTapEvent(kIgnoredPoint, 2);
  }

  void OnTripleTapEvent() { controller().HandleTapEvent(kIgnoredPoint, 3); }

  void Animate() {
    base::TimeTicks now = base::TimeTicks::Now();
    while (needs_animate_) {
      needs_animate_ = controller_->Animate(now);
      now += base::Milliseconds(16);
    }
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    Animate();
    return needs_animate;
  }

  bool GetAndResetCaretMoved() {
    bool moved = caret_moved_;
    caret_moved_ = false;
    return moved;
  }

  bool GetAndResetSelectionMoved() {
    bool moved = selection_moved_;
    selection_moved_ = false;
    return moved;
  }

  bool GetAndResetSelectionPointsSwapped() {
    bool swapped = selection_points_swapped_;
    selection_points_swapped_ = false;
    return swapped;
  }

  const gfx::PointF& GetLastCaretPosition() const { return caret_position_; }
  const gfx::PointF& GetLastSelectionStart() const { return selection_start_; }
  const gfx::PointF& GetLastSelectionEnd() const { return selection_end_; }
  const gfx::PointF& GetLastEventStart() const { return last_event_start_; }
  const gfx::PointF& GetLastEventEnd() const { return last_event_end_; }
  const gfx::RectF& GetLastEventBoundsRect() const {
    return last_event_bounds_rect_;
  }
  const gfx::PointF& GetLastDragUpdatePosition() const {
    return last_drag_update_position_;
  }

  std::vector<SelectionEventType> GetAndResetEvents() {
    std::vector<SelectionEventType> events;
    events.swap(events_);
    return events;
  }

  TouchSelectionController& controller() { return *controller_; }

 private:
  gfx::PointF last_event_start_;
  gfx::PointF last_event_end_;
  gfx::PointF caret_position_;
  gfx::PointF selection_start_;
  gfx::PointF selection_end_;
  gfx::RectF last_event_bounds_rect_;
  gfx::PointF last_drag_update_position_;
  std::vector<SelectionEventType> events_;
  bool caret_moved_ = false;
  bool selection_moved_ = false;
  bool selection_points_swapped_ = false;
  bool needs_animate_ = false;
  bool animation_enabled_ = true;
  bool dragging_enabled_ = false;
  std::unique_ptr<TouchSelectionController> controller_;
};

TEST_F(TouchSelectionControllerTest, InsertionBasic) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  insertion_rect.Offset(1, 0);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  insertion_rect.Offset(0, 1);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  OnTapEvent();
  insertion_rect.Offset(1, 0);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  ClearInsertion();
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED));
}

TEST_F(TouchSelectionControllerTest, InsertionToSelectionTransition) {
  OnLongPressEvent();

  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  ChangeInsertion(end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_CLEARED, INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventStart());

  ClearInsertion();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_CLEARED));

  OnTapEvent();
  ChangeInsertion(end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventStart());
}

TEST_F(TouchSelectionControllerTest, InsertionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnTapEvent();

  // The touch sequence should not be handled if insertion is not active.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  bool visible = true;
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED));

  // The MoveCaret() result should reflect the movement.
  // The reported position is offset from the center of |start_rect|.
  gfx::PointF start_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(5, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 10);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(10, 10), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STOPPED));

  // Following Action::DOWN should not be consumed if it does not start handle
  // dragging.
  SetDraggingEnabled(false);
  event = MockMotionEvent(MotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, InsertionDeactivatedWhileDragging) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnTapEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  bool visible = true;
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // Enable dragging so that the following Action::DOWN starts handle dragging.
  SetDraggingEnabled(true);

  // Touch down to start dragging.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED));

  // Move the handle.
  gfx::PointF start_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastCaretPosition());

  // Deactivate touch selection to end dragging.
  controller().HideAndDisallowShowingAutomatically();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STOPPED,
                                               INSERTION_HANDLE_CLEARED));

  // Move the finger. There is no handle to move, so the cursor is not moved;
  // but, the event is still consumed because the touch down that started the
  // touch sequence was consumed.
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastCaretPosition());

  // Lift the finger to end the touch sequence.
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  // Following Action::DOWN should not be consumed if it does not start handle
  // dragging.
  SetDraggingEnabled(false);
  event = MockMotionEvent(MotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, InsertionTapped) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnTapEvent();
  SetDraggingEnabled(true);

  gfx::RectF start_rect(10, 0, 0, 10);
  bool visible = true;
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_SHOWN));

  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED));

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_TAPPED,
                                               INSERTION_HANDLE_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, INSERTION_HANDLE_SHOWN));

  // No tap should be signalled if the time between DOWN and UP was too long.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::UP,
                          event_time + base::Seconds(1), 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED,
                                               INSERTION_HANDLE_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, INSERTION_HANDLE_SHOWN));

  // No tap should be signalled if the drag was too long.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 100, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 100, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED,
                                               INSERTION_HANDLE_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, INSERTION_HANDLE_SHOWN));

  // No tap should be signalled if the touch sequence is cancelled.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::CANCEL, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED,
                                               INSERTION_HANDLE_DRAG_STOPPED));
}

TEST_F(TouchSelectionControllerTest, SelectionBasic) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  start_rect.Offset(1, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  // Selection movement does not currently trigger a separate event.
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventEnd());

  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_CLEARED));
}

TEST_F(TouchSelectionControllerTest, SelectionAllowedByDoubleTap) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  OnDoubleTapEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
}

TEST_F(TouchSelectionControllerTest, SelectionAllowedByDoubleTapOnEditable) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  // If the user double tap selects text in an editable region, the first tap
  // will register insertion and the second tap selection.
  OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_SHOWN));

  OnDoubleTapEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, SELECTION_HANDLES_SHOWN));
}

TEST_F(TouchSelectionControllerTest,
       SelectionAllowedByTripleTapOnEditableArabicVowel) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(5, 5, 0, 10);
  bool visible = true;

  // If the user triple tap selects text in an editable region, the first tap
  // will register insertion.
  OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));

  // The second tap will also not select since the charcter (Arabic/Urdu vowel)
  // has zero width, the second tap will maintain insertion.
  OnDoubleTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre());

  // The third tap selects everything in the editable text box. Since the only
  // text in the editable box is a zero length character the selection has the
  // same start and end rect.
  OnTripleTapEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(INSERTION_HANDLE_CLEARED, SELECTION_HANDLES_SHOWN));
}

TEST_F(TouchSelectionControllerTest, SelectionAllowsEmptyUpdateAfterLongPress) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  OnLongPressEvent();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  // There may be several empty updates after a longpress due to the
  // asynchronous response. These empty updates should not prevent the selection
  // handles from (eventually) activating.
  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
}

TEST_F(TouchSelectionControllerTest, SelectionRepeatedLongPress) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventEnd());

  // A long press triggering a new selection should re-send the
  // SELECTION_HANDLES_SHOWN
  // event notification.
  start_rect.Offset(10, 10);
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventEnd());
}

TEST_F(TouchSelectionControllerTest, SelectionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  // The touch sequence should not be handled if selection is not active.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF start_rect(0, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // The SelectBetweenCoordinates() result should reflect the movement. Note
  // that the start coordinate will always reflect the "fixed" handle's
  // position, in this case the position from |end_rect|.
  // Note that the reported position is offset from the center of the
  // input rects (i.e., the middle of the corresponding text line).
  gfx::PointF fixed_offset = end_rect.CenterPoint();
  gfx::PointF start_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(5, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(10, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // Following Action::DOWN should not be consumed if it does not start handle
  // dragging.
  SetDraggingEnabled(false);
  event = MockMotionEvent(MotionEvent::Action::DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, SelectionDraggedWithOverlap) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(0, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The Action::DOWN should lock to the closest handle.
  gfx::PointF end_offset = end_rect.CenterPoint();
  gfx::PointF fixed_offset = start_rect.CenterPoint();
  float touch_down_x = (end_offset.x() + fixed_offset.x()) / 2 + 1.f;
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, touch_down_x,
                        0);
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // Even though the Action::MOVE is over the start handle, it should continue
  // targetting the end handle that consumed the Action::DOWN.
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(end_offset - gfx::Vector2dF(touch_down_x, 0),
            GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());
}

TEST_F(TouchSelectionControllerTest, SelectionDraggedToSwitchBaseAndExtent) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(50, line_height, 0, line_height);
  gfx::RectF end_rect(100, line_height, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  SetDraggingEnabled(true);

  // Move the extent, not triggering a swap of points.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, end_rect.x(),
                        end_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());

  gfx::PointF base_offset = start_rect.CenterPoint();
  gfx::PointF extent_offset = end_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          end_rect.x(), end_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  end_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));

  // Move the base, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          start_rect.x(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_TRUE(GetAndResetSelectionPointsSwapped());

  base_offset = end_rect.CenterPoint();
  extent_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          start_rect.x(), start_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  start_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));

  // Move the same point again, not triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          start_rect.x(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());

  base_offset = end_rect.CenterPoint();
  extent_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          start_rect.x(), start_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  start_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));

  // Move the base, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          end_rect.x(), end_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_TRUE(GetAndResetSelectionPointsSwapped());

  base_offset = start_rect.CenterPoint();
  extent_offset = end_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          end_rect.x(), end_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());
}

TEST_F(TouchSelectionControllerTest, SelectionDragExtremeLineSize) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  float small_line_height = 1.f;
  float large_line_height = 50.f;
  gfx::RectF small_line_rect(0, 0, 0, small_line_height);
  gfx::RectF large_line_rect(50, 50, 0, large_line_height);
  bool visible = true;
  ChangeSelection(small_line_rect, visible, large_line_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(small_line_rect.bottom_left(), GetLastEventStart());

  // Start dragging the handle on the small line.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time,
                        small_line_rect.x(), small_line_rect.y());
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  // The drag coordinate for large lines should be capped to a reasonable
  // offset, allowing seamless transition to neighboring lines with different
  // sizes. The drag coordinate for small lines should have an offset
  // commensurate with the small line size.
  EXPECT_EQ(large_line_rect.bottom_left() - gfx::Vector2dF(0, 8.f),
            GetLastSelectionStart());
  EXPECT_EQ(small_line_rect.CenterPoint(), GetLastSelectionEnd());

  small_line_rect += gfx::Vector2dF(25.f, 0);
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time,
                          small_line_rect.x(), small_line_rect.y());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(small_line_rect.CenterPoint(), GetLastSelectionEnd());
}

TEST_F(TouchSelectionControllerTest, Animation) {
  OnTapEvent();

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  // If the handles are explicity hidden, no animation should be triggered.
  controller().HideAndDisallowShowingAutomatically();
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  // If the client doesn't support animation, no animation should be triggered.
  SetAnimationEnabled(false);
  OnTapEvent();
  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchSelectionControllerTest, TemporarilyHidden) {
  TouchSelectionControllerTestApi test_controller(&controller());

  OnTapEvent();

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());

  controller().SetTemporarilyHidden(true);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());

  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());

  controller().SetTemporarilyHidden(false);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  EXPECT_TRUE(test_controller.GetStartVisible());
}

TEST_F(TouchSelectionControllerTest, SelectionClearOnTap) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));

  // Selection should not be cleared if the selection bounds have not changed.
  OnTapEvent();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  OnTapEvent();
  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_CLEARED));
}

TEST_F(TouchSelectionControllerTest, LongPressDrag) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.enable_longpress_drag_selection = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  gfx::RectF start_rect(-50, 0, 0, 10);
  gfx::RectF end_rect(50, 0, 0, 10);
  bool visible = true;

  // Start a touch sequence.
  MockMotionEvent event;
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.PressPoint(0, 0)));

  // Activate a longpress-triggered selection.
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The handles should remain invisible while the touch release and longpress
  // drag gesture are pending.
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  // The selection coordinates should reflect the drag movement.
  gfx::PointF fixed_offset = start_rect.CenterPoint();
  gfx::PointF end_offset = end_rect.CenterPoint();
  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, kDefaultTapSlop)));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(end_offset, GetLastSelectionEnd());

  // Movement after the start of drag will be relative to the moved endpoint.
  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(end_offset + gfx::Vector2dF(0, kDefaultTapSlop),
            GetLastSelectionEnd());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, kDefaultTapSlop, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(end_offset + gfx::Vector2dF(kDefaultTapSlop, kDefaultTapSlop),
            GetLastSelectionEnd());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 2 * kDefaultTapSlop, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(end_offset + gfx::Vector2dF(2 * kDefaultTapSlop, kDefaultTapSlop),
            GetLastSelectionEnd());

  // The handles should still be hidden.
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  // Releasing the touch sequence should end the drag and show the handles.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());
}

TEST_F(TouchSelectionControllerTest, DoublePressDrag) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.enable_longpress_drag_selection = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  // Start a touch sequence.
  MockMotionEvent event;
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.PressPoint(0, 0)));

  // Activate a double press triggered selection.
  constexpr gfx::RectF start_rect(-50, 0, 0, 10);
  constexpr gfx::RectF end_rect(50, 0, 0, 10);
  constexpr bool visible = true;
  OnDoublePressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The handles should remain invisible while the touch release and double
  // press drag gesture are pending.
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  // Start dragging.
  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, kDefaultTapSlop)));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(start_rect.CenterPoint(), GetLastSelectionStart());
  EXPECT_EQ(end_rect.CenterPoint(), GetLastSelectionEnd());

  // Movement after the start of drag will be relative to the moved endpoint.
  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(end_rect.CenterPoint() + gfx::Vector2dF(0, kDefaultTapSlop),
            GetLastSelectionEnd());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, kDefaultTapSlop, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(
      end_rect.CenterPoint() + gfx::Vector2dF(kDefaultTapSlop, kDefaultTapSlop),
      GetLastSelectionEnd());

  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 2 * kDefaultTapSlop, 2 * kDefaultTapSlop)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(end_rect.CenterPoint() +
                gfx::Vector2dF(2 * kDefaultTapSlop, kDefaultTapSlop),
            GetLastSelectionEnd());

  // The handles should still be hidden.
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  // Releasing the touch sequence should end the drag and show the handles.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());
}

TEST_F(TouchSelectionControllerTest, LongPressNoDrag) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.enable_longpress_drag_selection = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  gfx::RectF start_rect(-50, 0, 0, 10);
  gfx::RectF end_rect(50, 0, 0, 10);
  bool visible = true;

  // Start a touch sequence.
  MockMotionEvent event;
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.PressPoint(0, 0)));

  // Activate a longpress-triggered selection.
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  // The handles should remain invisible while the touch release and longpress
  // drag gesture are pending.
  EXPECT_FALSE(test_controller.GetStartVisible());
  EXPECT_FALSE(test_controller.GetEndVisible());

  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  // If no drag movement occurs, the handles should reappear after the touch
  // is released.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());
}

TEST_F(TouchSelectionControllerTest, NoLongPressDragIfDisabled) {
  // The TouchSelectionController disables longpress drag selection by default.
  InitializeControllerWithConfig(kDefaultConfig);
  TouchSelectionControllerTestApi test_controller(&controller());

  gfx::RectF start_rect(-50, 0, 0, 10);
  gfx::RectF end_rect(50, 0, 0, 10);
  bool visible = true;

  // Start a touch sequence.
  MockMotionEvent event;
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.PressPoint(0, 0)));

  // Activate a longpress-triggered selection.
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());

  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  // Subsequent motion of the same touch sequence after longpress shouldn't
  // trigger drag selection.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  EXPECT_FALSE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, kDefaultTapSlop * 10)));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  // Releasing the touch sequence should have no effect.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.ReleasePoint()));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_TRUE(test_controller.GetStartVisible());
  EXPECT_TRUE(test_controller.GetEndVisible());

  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());
}

TEST_F(TouchSelectionControllerTest, InsertionFocusBound) {
  OnTapEvent();

  // Activate insertion. Focus should be the caret.
  gfx::SelectionBound caret_bound;
  caret_bound.set_type(gfx::SelectionBound::CENTER);
  caret_bound.SetEdge(gfx::PointF(5, 5), gfx::PointF(5, 15));
  controller().OnSelectionBoundsChanged(caret_bound, caret_bound);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(caret_bound, controller().GetFocusBound());

  // Move caret.
  caret_bound.SetEdge(gfx::PointF(8, 5), gfx::PointF(8, 15));
  controller().OnSelectionBoundsChanged(caret_bound, caret_bound);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  EXPECT_EQ(caret_bound, controller().GetFocusBound());

  ClearInsertion();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_CLEARED));
}

TEST_F(TouchSelectionControllerTest, SelectionFocusBound) {
  OnLongPressEvent();

  // Activate selection. Focus should be the selection end.
  gfx::SelectionBound start_bound;
  start_bound.set_type(gfx::SelectionBound::LEFT);
  start_bound.SetEdge(gfx::PointF(5, 5), gfx::PointF(5, 15));
  gfx::SelectionBound end_bound;
  end_bound.set_type(gfx::SelectionBound::RIGHT);
  end_bound.SetEdge(gfx::PointF(50, 5), gfx::PointF(50, 15));
  controller().OnSelectionBoundsChanged(start_bound, end_bound);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(end_bound, controller().GetFocusBound());

  // Move the selection start. Focus should now be the selection start.
  start_bound.SetEdge(gfx::PointF(8, 5), gfx::PointF(8, 15));
  controller().OnSelectionBoundsChanged(start_bound, end_bound);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(start_bound, controller().GetFocusBound());

  // Move the selection end. Focus should now be the selection end.
  end_bound.SetEdge(gfx::PointF(52, 5), gfx::PointF(52, 15));
  controller().OnSelectionBoundsChanged(start_bound, end_bound);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(end_bound, controller().GetFocusBound());

  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_CLEARED));
}

TEST_F(TouchSelectionControllerTest, RectBetweenBounds) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  EXPECT_EQ(gfx::RectF(), controller().GetRectBetweenBounds());

  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  ASSERT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(gfx::RectF(5, 5, 45, 10), controller().GetRectBetweenBounds());

  // The result of |GetRectBetweenBounds| should be available within the
  // |OnSelectionEvent| callback, as stored by |GetLastEventBoundsRect()|.
  EXPECT_EQ(GetLastEventBoundsRect(), controller().GetRectBetweenBounds());

  start_rect.Offset(1, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(gfx::RectF(6, 5, 44, 10), controller().GetRectBetweenBounds());
  EXPECT_EQ(GetLastEventBoundsRect(), controller().GetRectBetweenBounds());

  // If only one bound is visible, the selection bounding rect should reflect
  // only the visible bound.
  ChangeSelection(start_rect, visible, end_rect, false);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(start_rect, controller().GetRectBetweenBounds());
  EXPECT_EQ(GetLastEventBoundsRect(), controller().GetRectBetweenBounds());

  ChangeSelection(start_rect, false, end_rect, visible);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(end_rect, controller().GetRectBetweenBounds());
  EXPECT_EQ(GetLastEventBoundsRect(), controller().GetRectBetweenBounds());

  // If both bounds are visible, the full bounding rect should be returned.
  ChangeSelection(start_rect, false, end_rect, false);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(gfx::RectF(6, 5, 44, 10), controller().GetRectBetweenBounds());
  EXPECT_EQ(GetLastEventBoundsRect(), controller().GetRectBetweenBounds());

  ClearSelection();
  ASSERT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_CLEARED));
  EXPECT_EQ(gfx::RectF(), controller().GetRectBetweenBounds());
}

TEST_F(TouchSelectionControllerTest, TouchHandleHeight) {
  OnLongPressEvent();
  SetDraggingEnabled(true);

  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);

  // Handle height should be zero when there is no selection/ insertion.
  EXPECT_EQ(0.f, controller().GetTouchHandleHeight());

  // Insertion case - Handle shown.
  ChangeInsertion(start_rect, true);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_NE(0.f, controller().GetTouchHandleHeight());

  // Insertion case - Handle moved.
  start_rect.Offset(1, 0);
  ChangeInsertion(start_rect, true);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  EXPECT_NE(0.f, controller().GetTouchHandleHeight());

  ClearInsertion();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_CLEARED));
  EXPECT_EQ(0.f, controller().GetTouchHandleHeight());

  // Selection case - Start and End are visible.
  ChangeSelection(start_rect, true, end_rect, true);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_NE(0.f, controller().GetTouchHandleHeight());

  // Selection case - Only Start is visible.
  ChangeSelection(start_rect, true, end_rect, false);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_NE(0.f, controller().GetTouchHandleHeight());

  // Selection case - Only End is visible.
  ChangeSelection(start_rect, false, end_rect, true);
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_NE(0.f, controller().GetTouchHandleHeight());

  ClearSelection();
  ASSERT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_CLEARED));
  EXPECT_EQ(0.f, controller().GetTouchHandleHeight());
}

TEST_F(TouchSelectionControllerTest, SelectionNoOrientationChangeWhenSwapped) {
  TouchSelectionControllerTestApi test_controller(&controller());
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(50, line_height, 0, line_height);
  gfx::RectF end_rect(100, line_height, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(),
              ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  SetDraggingEnabled(true);

  // Simulate moving the base, not triggering a swap of points.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time,
                        start_rect.x(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  gfx::RectF offset_rect = end_rect;
  offset_rect.Offset(gfx::Vector2dF(-10, 0));
  ChangeSelection(offset_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  // Simulate moving the base, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  offset_rect.Offset(gfx::Vector2dF(20, 0));
  ChangeSelection(end_rect, visible, offset_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::LEFT);

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  // Simulate moving the anchor, not triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  offset_rect.Offset(gfx::Vector2dF(-5, 0));
  ChangeSelection(end_rect, visible, offset_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  // Simulate moving the anchor, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  offset_rect.Offset(gfx::Vector2dF(-15, 0));
  ChangeSelection(offset_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::RIGHT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);
}

TEST_F(TouchSelectionControllerTest, VerticalTextSelectionHandleSwap) {
  TouchSelectionControllerTestApi test_controller(&controller());
  base::TimeTicks event_time = base::TimeTicks::Now();
  OnLongPressEvent();

  // Horizontal bounds.
  gfx::RectF start_rect(0, 50, 16, 0);
  gfx::RectF end_rect(0, 100, 16, 0);

  bool visible = true;
  ChangeVerticalSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_right(), GetLastEventStart());
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::RIGHT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::LEFT);

  SetDraggingEnabled(true);

  // Simulate moving the base, triggering a swap of points.
  // Start to drag start handle.
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time,
                        start_rect.right(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  // Move start handle down below end handle.
  gfx::RectF offset_rect = end_rect;
  offset_rect.Offset(gfx::Vector2dF(0, 20));
  ChangeVerticalSelection(end_rect, visible, offset_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::RIGHT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::RIGHT);

  // Release.
  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::RIGHT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::LEFT);

  // Move end handle up.
  // Start to drag end handle.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));

  // Move up end handle up above the start handle.
  offset_rect = start_rect;
  ChangeVerticalSelection(offset_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::LEFT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::LEFT);

  // Release.
  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time,
                          offset_rect.x(), offset_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
  EXPECT_EQ(test_controller.GetStartHandleOrientation(),
            TouchHandleOrientation::RIGHT);
  EXPECT_EQ(test_controller.GetEndHandleOrientation(),
            TouchHandleOrientation::LEFT);
}

TEST_F(TouchSelectionControllerTest, InsertionUpdateDragPosition) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF insertion_rect(10, 0, 0, line_height);
  bool visible = true;

  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(gfx::PointF(0.f, 0.f), GetLastDragUpdatePosition());

  SetDraggingEnabled(true);
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(gfx::PointF(10.f, 5.f), GetLastDragUpdatePosition());

  insertion_rect.Offset(1, 0);
  ChangeInsertion(insertion_rect, visible);
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 12, 6);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  // Don't follow the y-coordinate change but only x-coordinate change.
  EXPECT_EQ(gfx::PointF(12.f, 5.f), GetLastDragUpdatePosition());

  insertion_rect.Offset(0, 1);
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 11, 6);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  ChangeInsertion(insertion_rect, visible);
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 11, 7);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_MOVED));
  // Don't follow the y-coordinate change.
  EXPECT_EQ(gfx::PointF(11.f, 6.f), GetLastDragUpdatePosition());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_DRAG_STOPPED));

  SetDraggingEnabled(false);
}

TEST_F(TouchSelectionControllerTest, SelectionUpdateDragPosition) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;
  OnLongPressEvent();

  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(gfx::PointF(0.f, 0.f), GetLastDragUpdatePosition());

  // Left handle.
  SetDraggingEnabled(true);
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(gfx::PointF(10.f, 5.f), GetLastDragUpdatePosition());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 16, 6);
  start_rect.Offset(5, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  // Don't follow the y-coordinate change but only x-coordinate change.
  EXPECT_EQ(gfx::PointF(16.f, 5.f), GetLastDragUpdatePosition());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 15, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));

  // Right handle.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(gfx::PointF(50.f, 5.f), GetLastDragUpdatePosition());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 45, 5);
  end_rect.Offset(-5, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_MOVED));
  EXPECT_EQ(gfx::PointF(45.f, 5.f), GetLastDragUpdatePosition());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 45, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STOPPED));
}

TEST_F(TouchSelectionControllerTest, LongpressDragSelectorUpdateDragPosition) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.enable_longpress_drag_selection = true;
  InitializeControllerWithConfig(config);
  float line_height = 10.f;
  gfx::RectF start_rect(-40, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;

  // Start a touch sequence.
  MockMotionEvent event;
  EXPECT_FALSE(controller().WillHandleTouchEvent(event.PressPoint(0, 0)));

  // Activate a longpress-triggered selection
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLES_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventStart());

  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 0, 0)));
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  // Move within tap slop, move haven't started yet.
  EXPECT_TRUE(controller().WillHandleTouchEvent(
      event.MovePoint(0, 0, kDefaultTapSlop)));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_HANDLE_DRAG_STARTED));
  EXPECT_EQ(gfx::PointF(0.f, 0.f), GetLastDragUpdatePosition());

  // Movement after the start of drag will be relative to the moved endpoint,
  // the actual selection change offset is not necessary equal to the event
  // moving distance.
  end_rect.Offset(6, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 5, 0)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(gfx::PointF(56.f, 5.f), GetLastDragUpdatePosition());

  // Vertical move
  end_rect.Offset(0, 10);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 5, 10)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(gfx::PointF(56.f, 15.f), GetLastDragUpdatePosition());

  // Move start
  start_rect.Offset(30, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event.MovePoint(0, 35, 10)));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(gfx::PointF(-10.f, 5.f), GetLastDragUpdatePosition());
}

TEST_F(TouchSelectionControllerTest, NoHideActiveInsertionHandle) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = false;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF insertion_rect(10, 0, 0, line_height);
  bool visible = true;

  StartTouchEventSequence();
  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));

  SetDraggingEnabled(true);
  EXPECT_EQ(1.f, test_controller.GetInsertionHandleAlpha());
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetInsertionHandleAlpha());
}

TEST_F(TouchSelectionControllerTest, HideActiveInsertionHandle) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF insertion_rect(10, 0, 0, line_height);
  bool visible = true;

  StartTouchEventSequence();
  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));

  SetDraggingEnabled(true);
  EXPECT_EQ(1.f, test_controller.GetInsertionHandleAlpha());
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(0.f, test_controller.GetInsertionHandleAlpha());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(0.f, test_controller.GetInsertionHandleAlpha());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  // UP will reset the alpha to visible.
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetInsertionHandleAlpha());
}

TEST_F(TouchSelectionControllerTest, NoHideActiveSelectionHandle) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = false;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;

  StartTouchEventSequence();
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);

  // Start handle.
  SetDraggingEnabled(true);
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  // End handle.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());
}

TEST_F(TouchSelectionControllerTest, HideActiveSelectionHandle) {
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  base::TimeTicks event_time = base::TimeTicks::Now();
  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;

  StartTouchEventSequence();
  OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);

  // Start handle.
  SetDraggingEnabled(true);
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());
  MockMotionEvent event(MockMotionEvent::Action::DOWN, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(0.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  // UP will reset alpha to be visible.
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());

  // End handle.
  event = MockMotionEvent(MockMotionEvent::Action::DOWN, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  event = MockMotionEvent(MockMotionEvent::Action::MOVE, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));

  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(0.f, test_controller.GetEndAlpha());

  event_time += base::Milliseconds(2 * kDefaultTapTimeoutMs);
  // UP will reset alpha to be visible.
  event = MockMotionEvent(MockMotionEvent::Action::UP, event_time, 50, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(1.f, test_controller.GetStartAlpha());
  EXPECT_EQ(1.f, test_controller.GetEndAlpha());
}

TEST_F(TouchSelectionControllerTest, SwipeToMoveCursor_HideHandlesIfShown) {
  // Step 1: Extra set-up.
  // For Android P+, we need to hide handles while showing magnifier.
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  StartTouchEventSequence();
  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  EXPECT_TRUE(test_controller.GetStartVisible());

  // Step 2: Swipe-to-move-cursor begins: hide handles.
  controller().OnSwipeToMoveCursorBegin();
  EXPECT_FALSE(test_controller.GetStartVisible());

  // Step 3: Move insertion: still hidden.
  gfx::RectF new_insertion_rect(10, 5, 0, 10);
  ChangeInsertion(new_insertion_rect, visible);
  EXPECT_FALSE(test_controller.GetStartVisible());

  // Step 4: Swipe-to-move-cursor ends: show handles.
  controller().OnSwipeToMoveCursorEnd();
  EXPECT_TRUE(test_controller.GetStartVisible());
}

TEST_F(TouchSelectionControllerTest, SwipeToMoveCursor_HandleWasNotShown) {
  // Step 1: Extra set-up.
  // For Android P+, we need to hide handles while showing magnifier.
  TouchSelectionController::Config config = kDefaultConfig;
  config.hide_active_handle = true;
  InitializeControllerWithConfig(config);
  TouchSelectionControllerTestApi test_controller(&controller());

  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  StartTouchEventSequence();
  OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_HANDLE_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventStart());

  EXPECT_TRUE(test_controller.GetStartVisible());

  // Step 2: Handle is initially hidden, i.e., due to user typing.
  controller().HideAndDisallowShowingAutomatically();
  EXPECT_FALSE(test_controller.GetStartVisible());

  // Step 3: Swipe-to-move-cursor begins: hide handles.
  controller().OnSwipeToMoveCursorBegin();
  EXPECT_FALSE(test_controller.GetStartVisible());

  // Step 4: Move insertion.
  // Note that this step is needed to show handle at the end since
  // OnInsertionChanged() should activate start_ again, although it will stay
  // temporarily hidden.
  gfx::RectF new_insertion_rect(10, 5, 0, 10);
  ChangeInsertion(new_insertion_rect, visible);
  EXPECT_FALSE(test_controller.GetStartVisible());

  // Step 5: Swipe-to-move-cursor ends: show handles.
  controller().OnSwipeToMoveCursorEnd();
  EXPECT_TRUE(test_controller.GetStartVisible());
}

}  // namespace
}  // namespace ui
