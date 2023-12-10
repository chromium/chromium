// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/longpress_drag_selector.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {
class MotionEvent;
class Event;

// Interface through which |TouchSelectionController| issues selection-related
// commands, notifications and requests.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionControllerClient {
 public:
  virtual ~TouchSelectionControllerClient() {}

  virtual bool SupportsAnimation() const = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual void MoveCaret(const gfx::PointF& position) = 0;
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) = 0;
  virtual void SelectBetweenCoordinates(const gfx::PointF& base,
                                        const gfx::PointF& extent) = 0;
  virtual void OnSelectionEvent(SelectionEventType event) = 0;
  virtual void OnDragUpdate(const TouchSelectionDraggable::Type type,
                            const gfx::PointF& position) = 0;
  virtual std::unique_ptr<TouchHandleDrawable> CreateDrawable() = 0;
  virtual void DidScroll() = 0;
  virtual void ShowTouchSelectionContextMenu(const gfx::Point& location) {}
};

// Controller for manipulating text selection via touch input.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionController
    : public TouchHandleClient,
      public LongPressDragSelectorClient {
 public:
  enum ActiveStatus {
    INACTIVE,
    INSERTION_ACTIVE,
    SELECTION_ACTIVE,
  };

  struct UI_TOUCH_SELECTION_EXPORT Config {
    // Maximum allowed time for handle tap detection. Defaults to 300 ms.
    base::TimeDelta max_tap_duration = base::Milliseconds(300);

    // Defaults to 8 DIPs.
    float tap_slop = 8;

    // Controls whether adaptive orientation for selection handles is enabled.
    // Defaults to false.
    bool enable_adaptive_handle_orientation = false;

    // Controls whether drag selection after a longpress is enabled.
    // Defaults to false.
    bool enable_longpress_drag_selection = false;

    // Should we hide the active handle.
    bool hide_active_handle = false;
  };

  TouchSelectionController(TouchSelectionControllerClient* client,
                           const Config& config);

  TouchSelectionController(const TouchSelectionController&) = delete;
  TouchSelectionController& operator=(const TouchSelectionController&) = delete;

  ~TouchSelectionController() override;

  // To be called when the selection bounds have changed.
  // Note that such updates will trigger handle updates only if preceded
  // by an appropriate call to allow automatic showing.
  void OnSelectionBoundsChanged(const gfx::SelectionBound& start,
                                const gfx::SelectionBound& end);

  // To be called when the viewport rect has been changed. This is used for
  // setting the state of the handles.
  void OnViewportChanged(const gfx::RectF viewport_rect);

  // Allows touch-dragging of the handle.
  // Returns true iff the event was consumed, in which case the caller should
  // cease further handling of the event.
  bool WillHandleTouchEvent(const MotionEvent& event);

  // To be called before forwarding a tap event.
  // |tap_count| is tap index in a repeated sequence, i.e., 1 for the first
  // tap, 2 for the second tap, etc...
  void HandleTapEvent(const gfx::PointF& location, int tap_count);

  // To be called before forwarding a longpress event.
  void HandleLongPressEvent(base::TimeTicks event_time,
                                const gfx::PointF& location);

  // To be called before forwarding a double press event.
  void HandleDoublePressEvent(base::TimeTicks event_time,
                              const gfx::PointF& location);

  // To be called before forwarding a gesture scroll begin event to prevent
  // long-press drag.
  void OnScrollBeginEvent();

  // To be called when a menu command has been requested, to dismiss touch
  // handles and record metrics if needed.
  void OnMenuCommand(bool should_dismiss_handles);

  // To be called when an event occurs to deactivate touch selection.
  void OnSessionEndEvent(const Event& event);

  // Hide the handles and suppress bounds updates until the next explicit
  // showing allowance.
  void HideAndDisallowShowingAutomatically();

  // Override the handle visibility according to |hidden|.
  void SetTemporarilyHidden(bool hidden);

  // Ticks an active animation, as requested to the client by |SetNeedsAnimate|.
  // Returns true if an animation is active and requires further ticking.
  bool Animate(base::TimeTicks animate_time);

  // Returns the current focus bound. For an active selection, this is the
  // selection bound that has most recently been dragged or updated (defaulting
  // to the end if neither endpoint has moved). For an active insertion it is
  // the caret bound. Should only be called when touch selection is active.
  const gfx::SelectionBound& GetFocusBound() const;

  // Returns the rect between the two active selection bounds. If just one of
  // the bounds is visible, or both bounds are visible and on the same line,
  // the rect is simply a one-dimensional rect of that bound. If no selection
  // is active, an empty rect will be returned.
  gfx::RectF GetRectBetweenBounds() const;
  // Returns the rect between the selection bounds (as above) but clipped by
  // occluding layers.
  gfx::RectF GetVisibleRectBetweenBounds() const;

  // Returns the visible rect of specified touch handle. For an active insertion
  // these values will be identical.
  gfx::RectF GetStartHandleRect() const;
  gfx::RectF GetEndHandleRect() const;

  // Return the handle height of visible touch handle. This value will be zero
  // when no handle is visible.
  float GetTouchHandleHeight() const;

  // Returns the focal point of the start and end bounds, as defined by
  // their bottom coordinate.
  const gfx::PointF& GetStartPosition() const;
  const gfx::PointF& GetEndPosition() const;

  // To be called when swipe-to-move-cursor motion begins.
  void OnSwipeToMoveCursorBegin();
  // To be called when swipe-to-move-cursor motion ends.
  void OnSwipeToMoveCursorEnd();

  const gfx::SelectionBound& start() const { return start_; }
  const gfx::SelectionBound& end() const { return end_; }

  ActiveStatus active_status() const { return active_status_; }

 private:
  friend class TouchSelectionControllerTestApi;

  enum InputEventType { TAP, REPEATED_TAP, LONG_PRESS, INPUT_EVENT_TYPE_NONE };

  enum class DragSelectorInitiatingGesture { kNone, kLongPress, kDoublePress };

  bool WillHandleTouchEventImpl(const MotionEvent& event);

  // TouchHandleClient implementation.
  void OnDragBegin(const TouchSelectionDraggable& draggable,
                   const gfx::PointF& drag_position) override;
  void OnDragUpdate(const TouchSelectionDraggable& draggable,
                    const gfx::PointF& drag_position) override;
  void OnDragEnd(const TouchSelectionDraggable& draggable) override;
  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override;
  void OnHandleTapped(const TouchHandle& handle) override;
  void SetNeedsAnimate() override;
  std::unique_ptr<TouchHandleDrawable> CreateDrawable() override;
  base::TimeDelta GetMaxTapDuration() const override;
  bool IsAdaptiveHandleOrientationEnabled() const override;

  // LongPressDragSelectorClient implementation.
  void OnLongPressDragActiveStateChanged() override;
  gfx::PointF GetSelectionStart() const override;
  gfx::PointF GetSelectionEnd() const override;

  void OnInsertionChanged();
  void OnSelectionChanged();

  // Returns true if insertion mode was newly (re)activated.
  bool ActivateInsertionIfNecessary();
  void DeactivateInsertion();
  // Returns true if selection mode was newly (re)activated.
  bool ActivateSelectionIfNecessary();
  void DeactivateSelection();
  void UpdateHandleLayoutIfNecessary();

  bool WillHandleTouchEventForLongPressDrag(const MotionEvent& event);
  void SetTemporarilyHiddenForLongPressDrag(bool hidden);
  void RefreshHandleVisibility();

  // Returns the y-coordinate of middle point of selection bound corresponding
  // to the active selection or insertion handle. If there is no active handle,
  // returns 0.0.
  float GetActiveHandleMiddleY() const;

  void HideHandles();

  gfx::Vector2dF GetStartLineOffset() const;
  gfx::Vector2dF GetEndLineOffset() const;
  bool GetStartVisible() const;
  bool GetEndVisible() const;
  TouchHandle::AnimationStyle GetAnimationStyle(bool was_active) const;

  void LogDragType(const TouchSelectionDraggable& draggable);

  const raw_ptr<TouchSelectionControllerClient, DanglingUntriaged> client_;
  const Config config_;

  InputEventType response_pending_input_event_;

  // The bounds at the begin and end of the selection, which might be vertical
  // or horizontal line and represents the position of the touch handles or
  // caret.
  gfx::SelectionBound start_;
  gfx::SelectionBound end_;
  TouchHandleOrientation start_orientation_;
  TouchHandleOrientation end_orientation_;

  ActiveStatus active_status_;

  std::unique_ptr<TouchHandle> insertion_handle_;

  std::unique_ptr<TouchHandle> start_selection_handle_;
  std::unique_ptr<TouchHandle> end_selection_handle_;

  bool temporarily_hidden_;

  // Whether to use the start bound (if false, the end bound) for computing the
  // appropriate text line offset when performing a selection drag. This helps
  // ensure that the initial selection induced by the drag doesn't "jump"
  // between lines.
  bool anchor_drag_to_selection_start_;

  // Allows the text selection to be adjusted by touch dragging after a long
  // press or double press initiated selection.
  LongPressDragSelector longpress_drag_selector_;

  // Used to track whether a selection drag gesture was initiated by a long
  // press or double press.
  DragSelectorInitiatingGesture drag_selector_initiating_gesture_ =
      DragSelectorInitiatingGesture::kNone;

  gfx::RectF viewport_rect_;

  // Whether a selection handle was dragged during the current 'selection
  // session' - i.e. since the current selection has been activated.
  bool selection_handle_dragged_;

  // Determines whether the entire touch sequence should be consumed or not.
  bool consume_touch_sequence_;

  bool show_touch_handles_;

  TouchSelectionSessionMetricsRecorder session_metrics_recorder_;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_
