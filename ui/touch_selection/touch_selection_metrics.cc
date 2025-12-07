// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "ui/touch_selection/touch_editing_controller.h"

namespace ui {

namespace {

TouchSelectionMenuAction MapCommandIdToMenuAction(int command_id) {
  switch (command_id) {
    case TouchEditable::kCut:
      return TouchSelectionMenuAction::kCut;
    case TouchEditable::kCopy:
      return TouchSelectionMenuAction::kCopy;
    case TouchEditable::kPaste:
      return TouchSelectionMenuAction::kPaste;
    case TouchEditable::kSelectAll:
      return TouchSelectionMenuAction::kSelectAll;
    case TouchEditable::kSelectWord:
      return TouchSelectionMenuAction::kSelectWord;
    default:
      NOTREACHED() << "Invalid command id: " << command_id;
  }
}

}  // namespace

void RecordTouchSelectionDrag(TouchSelectionDragType drag_type) {
  base::UmaHistogramEnumeration(kTouchSelectionDragTypeHistogramName,
                                drag_type);
}

void RecordTouchSelectionMenuCommandAction(int command_id) {
  base::UmaHistogramEnumeration(kTouchSelectionMenuActionHistogramName,
                                MapCommandIdToMenuAction(command_id));
}

void RecordTouchSelectionMenuEllipsisAction() {
  base::UmaHistogramEnumeration(kTouchSelectionMenuActionHistogramName,
                                TouchSelectionMenuAction::kEllipsis);
}

void RecordTouchSelectionMenuSmartAction() {
  base::UmaHistogramEnumeration(kTouchSelectionMenuActionHistogramName,
                                TouchSelectionMenuAction::kSmartAction);
}

}  // namespace ui
