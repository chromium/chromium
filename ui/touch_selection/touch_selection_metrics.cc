// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/events/event.h"

namespace ui {

namespace {

constexpr int kSessionTouchDownCountMin = 1;
constexpr int kSessionTouchDownCountMax = 20;
constexpr int kSessionTouchDownCountBuckets = 20;

// Duration of inactivity after which we consider a touch selection session to
// have timed out for the purpose of determining session action count metrics.
constexpr base::TimeDelta kSessionTimeoutDuration = base::Seconds(10);

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

// We want to record the touch down count required to get to a successful cursor
// placement or selection, but it's hard to know if this has happened. We'll
// just consider a session to be successful if it ends in a character key event
// or an IME fabricated key event (e.g. from the ChromeOS virtual keyboard).
bool IsSuccessfulSessionEndEvent(const Event& session_end_event) {
  if (!session_end_event.IsKeyEvent()) {
    return false;
  }

  return session_end_event.AsKeyEvent()->GetDomKey().IsCharacter() ||
         session_end_event.flags() & EF_IME_FABRICATED_KEY;
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

TouchSelectionSessionMetricsRecorder::TouchSelectionSessionMetricsRecorder() =
    default;

TouchSelectionSessionMetricsRecorder::~TouchSelectionSessionMetricsRecorder() =
    default;

void TouchSelectionSessionMetricsRecorder::OnCursorActivationEvent() {
  if (!IsSessionActive()) {
    // We assume that an initial activation event occurs after a single touch
    // down movement (from a tap or long press). This is not always correct,
    // e.g. if the user double taps quickly enough then the cursor event from
    // the first tap might occur after the second tap was already detected. But
    // it should be ok to assume that this won't be a problem most of the time.
    session_touch_down_count_ = 1;
  }

  active_status_ = ActiveStatus::kActiveCursor;
}

void TouchSelectionSessionMetricsRecorder::OnSelectionActivationEvent() {
  if (!IsSessionActive()) {
    // We assume that an initial activation event occurs after a single touch
    // down movement (from a long press), since a selection event from a
    // repeated tap would usually only occur after a cursor event from the
    // first tap has already started the session.
    session_touch_down_count_ = 1;
  }

  active_status_ = ActiveStatus::kActiveSelection;
}

void TouchSelectionSessionMetricsRecorder::OnTouchEvent(bool is_down_event) {
  RefreshSessionStatus();
  if (!IsSessionActive()) {
    return;
  }

  session_touch_down_count_ += is_down_event;
}

void TouchSelectionSessionMetricsRecorder::OnMenuCommand(
    bool should_end_session) {
  RefreshSessionStatus();
  if (!IsSessionActive()) {
    return;
  }

  // We assume that a menu button was tapped, but only include this in the touch
  // down count if the session continues (since we want to know the touch down
  // count required to get to a successful cursor placement or selection, which
  // would have occurred before the menu button was tapped).
  if (should_end_session) {
    RecordSessionMetrics();
    ResetMetrics();
  } else {
    session_touch_down_count_++;
  }
}

void TouchSelectionSessionMetricsRecorder::OnSessionEndEvent(
    const Event& session_end_event) {
  RefreshSessionStatus();
  if (!IsSessionActive()) {
    return;
  }

  if (IsSuccessfulSessionEndEvent(session_end_event)) {
    RecordSessionMetrics();
  }
  ResetMetrics();
}

void TouchSelectionSessionMetricsRecorder::ResetMetrics() {
  active_status_ = ActiveStatus::kInactive;
  last_activity_time_ = base::TimeTicks();
  session_touch_down_count_ = 0;
}

void TouchSelectionSessionMetricsRecorder::RefreshSessionStatus() {
  // After a period of inactivity, we consider a session to have timed out since
  // the user intent has probably changed.
  if (last_activity_time_ + kSessionTimeoutDuration < base::TimeTicks::Now()) {
    ResetMetrics();
  }

  last_activity_time_ = base::TimeTicks::Now();
}

bool TouchSelectionSessionMetricsRecorder::IsSessionActive() const {
  return active_status_ != ActiveStatus::kInactive;
}

void TouchSelectionSessionMetricsRecorder::RecordSessionMetrics() const {
  if (!IsSessionActive()) {
    return;
  }

  base::UmaHistogramCustomCounts(
      active_status_ == ActiveStatus::kActiveCursor
          ? kTouchCursorSessionTouchDownCountHistogramName
          : kTouchSelectionSessionTouchDownCountHistogramName,
      session_touch_down_count_, kSessionTouchDownCountMin,
      kSessionTouchDownCountMax, kSessionTouchDownCountBuckets);
}

}  // namespace ui
