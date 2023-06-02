// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_LONGPRESS_DRAG_SELECTOR_H_
#define UI_TOUCH_SELECTION_LONGPRESS_DRAG_SELECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/touch_selection/touch_selection_draggable.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {

class MotionEvent;

class UI_TOUCH_SELECTION_EXPORT LongPressDragSelectorClient
    : public TouchSelectionDraggableClient {
 public:
  ~LongPressDragSelectorClient() override {}
  virtual void OnLongPressDragActiveStateChanged() = 0;
  virtual gfx::PointF GetSelectionStart() const = 0;
  virtual gfx::PointF GetSelectionEnd() const = 0;
};

// Supports text selection via touch dragging after a longpress- or
// doublepress-initiated selection.
class UI_TOUCH_SELECTION_EXPORT LongPressDragSelector
    : public TouchSelectionDraggable {
 public:
  explicit LongPressDragSelector(LongPressDragSelectorClient* client);
  ~LongPressDragSelector() override;

  // TouchSelectionDraggable implementation.
  bool WillHandleTouchEvent(const MotionEvent& event) override;
  bool IsActive() const override;

  // Called just prior to a longpress event being handled.
  void OnLongPressEvent(base::TimeTicks event_time,
                        const gfx::PointF& position);

  // Called just prior to a double press event being handled.
  void OnDoublePressEvent(base::TimeTicks event_time,
                          const gfx::PointF& position);

  // Called when a scroll is going to happen to cancel longpress-drag gesture.
  void OnScrollBeginEvent();

  // Called when the active selection changes.
  void OnSelectionActivated();
  void OnSelectionDeactivated();

 private:
  enum SelectionState {
    INACTIVE,
    INITIATING_GESTURE_PENDING,
    SELECTION_PENDING,
    DRAG_PENDING,
    DRAGGING
  };

  void SetState(SelectionState state);

  const raw_ptr<LongPressDragSelectorClient> client_;

  SelectionState state_;

  base::TimeTicks touch_down_time_;
  gfx::PointF touch_down_position_;

  gfx::Vector2dF longpress_drag_selection_offset_;
  gfx::PointF longpress_drag_start_anchor_;
  bool has_longpress_drag_start_anchor_;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_LONGPRESS_DRAG_SELECTOR_H_
