// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "ui/base/pointer/touch_editing_controller.h"

namespace ui {

namespace {

TouchSelectionMenuAction MapCommandIdToMenuAction(int command_id) {
  switch (command_id) {
    case ui::TouchEditable::kCut:
      return TouchSelectionMenuAction::kCut;
    case ui::TouchEditable::kCopy:
      return TouchSelectionMenuAction::kCopy;
    case ui::TouchEditable::kPaste:
      return TouchSelectionMenuAction::kPaste;
    case ui::TouchEditable::kSelectAll:
      return TouchSelectionMenuAction::kSelectAll;
    case ui::TouchEditable::kSelectWord:
      return TouchSelectionMenuAction::kSelectWord;
    default:
      NOTREACHED_NORETURN() << "Invalid command id: " << command_id;
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
