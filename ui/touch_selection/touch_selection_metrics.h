// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_

#include "base/time/time.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {
class Event;

inline constexpr char kTouchSelectionDragTypeHistogramName[] =
    "InputMethod.TouchSelection.DragType";

inline constexpr char kTouchSelectionMenuActionHistogramName[] =
    "InputMethod.TouchSelection.MenuAction";

inline constexpr char kTouchCursorSessionTouchDownCountHistogramName[] =
    "InputMethod.TouchSelection.CursorSession.TouchDownCount";

inline constexpr char kTouchSelectionSessionTouchDownCountHistogramName[] =
    "InputMethod.TouchSelection.SelectionSession.TouchDownCount";

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

// Helper class for tracking the state of touch selection sessions and recording
// session related metrics.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionSessionMetricsRecorder {
 public:
  TouchSelectionSessionMetricsRecorder();

  TouchSelectionSessionMetricsRecorder(
      const TouchSelectionSessionMetricsRecorder&) = delete;
  TouchSelectionSessionMetricsRecorder& operator=(
      const TouchSelectionSessionMetricsRecorder&) = delete;

  ~TouchSelectionSessionMetricsRecorder();

  // Called when the cursor or selection handles are shown or moved. This starts
  // the session if it is not yet active and updates the session type (cursor or
  // selection) if needed.
  void OnCursorActivationEvent();
  void OnSelectionActivationEvent();

  // Called when a touch event occurs, to update the session status and touch
  // down count. We assume this is only called for touch events targeting the
  // touch selection context window (e.g. for tapping on the text or dragging
  // touch handles, but not for tapping popup menu buttons).
  void OnTouchEvent(bool is_down_event);

  // Called when a menu command has been requested. If `should_end_session` is
  // true, the session ends and metrics are recorded. Otherwise, the touch down
  // count is incremented (since we assume a menu button was tapped) and the
  // session continues.
  void OnMenuCommand(bool should_end_session);

  // Called when an event occurs to deactivate touch selection. This ends the
  // session and records metrics if the session is deemed successful.
  void OnSessionEndEvent(const Event& session_end_event);

  // Resets the session state, effectively ending the session without recording
  // metrics.
  void ResetMetrics();

 private:
  enum class ActiveStatus {
    kInactive,
    kActiveCursor,
    kActiveSelection,
  };

  // Helper to be called when user activity is detected (e.g. touch event or
  // menu action), to check whether the session should continue or be considered
  // timed out.
  void RefreshSessionStatus();

  bool IsSessionActive() const;

  void RecordSessionMetrics() const;

  ActiveStatus active_status_ = ActiveStatus::kInactive;

  // The time of the most recently detected user activity.
  base::TimeTicks last_activity_time_ = base::TimeTicks();

  int session_touch_down_count_ = 0;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_METRICS_H_
