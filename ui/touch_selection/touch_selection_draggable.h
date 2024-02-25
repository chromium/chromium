// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_DRAGGABLE_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_DRAGGABLE_H_

#include "ui/gfx/geometry/point_f.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {

class MotionEvent;
class TouchSelectionDraggable;

// Interface through which TouchSelectionDraggable manipulates the selection.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionDraggableClient {
 public:
  virtual ~TouchSelectionDraggableClient() {}
  virtual void OnDragBegin(const TouchSelectionDraggable& draggable,
                           const gfx::PointF& start_position) = 0;
  virtual void OnDragUpdate(const TouchSelectionDraggable& draggable,
                            const gfx::PointF& new_position) = 0;
  virtual void OnDragEnd(const TouchSelectionDraggable& draggable) = 0;
  virtual bool IsWithinTapSlop(const gfx::Vector2dF& delta) const = 0;
};

// Generic interface for entities that manipulate the selection via dragging.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionDraggable {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.touch_selection
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: TouchSelectionDraggableType
  enum class Type {
    kNone,
    kTouchHandle,
    kLongpress,
  };

 protected:
  virtual ~TouchSelectionDraggable() {}

  // Offers a touch sequence to the draggable target. Returns true if the event
  // was consumed, in which case the caller should cease further handling.
  virtual bool WillHandleTouchEvent(const MotionEvent& event) = 0;

  // Whether a drag is active OR being detected for the current touch sequence.
  virtual bool IsActive() const = 0;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_DRAGGABLE_H_
