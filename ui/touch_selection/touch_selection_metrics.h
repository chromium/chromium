// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_

#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {

inline constexpr char kTouchSelectionDragTypeHistogramName[] =
    "InputMethod.TouchSelection.DragType";

inline constexpr char kTouchSelectionMenuActionHistogramName[] =
    "InputMethod.TouchSelection.MenuAction";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TouchSelectionDragType {
  kCursorHandleDrag = 0,
  kSelectionHandleDrag = 1,
  kCursorDrag = 2,
  kLongPressDrag = 3,
  kDoublePressDrag = 4,
  kMaxValue = kDoublePressDrag
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TouchSelectionMenuAction {
  kCut = 0,
  kCopy = 1,
  kPaste = 2,
  kSelectAll = 3,
  kSelectWord = 4,
  kEllipsis = 5,
  kSmartAction = 6,
  kMaxValue = kSmartAction
};

UI_TOUCH_SELECTION_EXPORT void RecordTouchSelectionDrag(
    TouchSelectionDragType drag_type);

UI_TOUCH_SELECTION_EXPORT void RecordTouchSelectionMenuCommandAction(
    int command_id);
UI_TOUCH_SELECTION_EXPORT void RecordTouchSelectionMenuEllipsisAction();
UI_TOUCH_SELECTION_EXPORT void RecordTouchSelectionMenuSmartAction();

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_
