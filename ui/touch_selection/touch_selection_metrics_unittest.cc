// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ui {

namespace {

TEST(TouchSelectionSessionMetricsTest, RecordsSuccessfulCursorSession) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate cursor, then end session successfully with a menu command.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnCursorActivationEvent();
  session_metrics_recorder.OnMenuCommand(true);

  histogram_tester.ExpectUniqueSample(
      kTouchCursorSessionTouchDownCountHistogramName, 1, 1);
}

TEST(TouchSelectionSessionMetricsTest, RecordsSuccessfulSelectionSession) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate selection, then end session successfully with a menu command.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.OnMenuCommand(true);

  histogram_tester.ExpectUniqueSample(
      kTouchSelectionSessionTouchDownCountHistogramName, 1, 1);
}

TEST(TouchSelectionSessionMetricsTest, DoesNotRecordDismissedSession) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate selection, then dismiss session.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.ResetMetrics();

  histogram_tester.ExpectTotalCount(
      kTouchSelectionSessionTouchDownCountHistogramName, 0);
}

TEST(TouchSelectionSessionMetricsTest, MultipleSessions) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate, then end session successfully with a menu command.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.OnMenuCommand(true);
  // Activate, then dismiss session.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.ResetMetrics();
  // Activate, then end another session successfully.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnMenuCommand(true);

  histogram_tester.ExpectTotalCount(
      kTouchSelectionSessionTouchDownCountHistogramName, 2);
  // First session.
  histogram_tester.ExpectBucketCount(
      kTouchSelectionSessionTouchDownCountHistogramName, 1, 1);
  // Third session.
  histogram_tester.ExpectBucketCount(
      kTouchSelectionSessionTouchDownCountHistogramName, 2, 1);
}

TEST(TouchSelectionSessionMetricsTest, MultipleActivationEvents) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate cursor.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnCursorActivationEvent();
  // Activate selection within the same session.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  // Perform some touch events.
  session_metrics_recorder.OnTouchEvent(false);
  session_metrics_recorder.OnTouchEvent(true);
  // End session successfully with a menu command.
  session_metrics_recorder.OnMenuCommand(true);

  // Selection session metrics should be recorded, since there was an active
  // selection when the session ended.
  histogram_tester.ExpectUniqueSample(
      kTouchSelectionSessionTouchDownCountHistogramName, 3, 1);
}

TEST(TouchSelectionSessionMetricsTest, RecordsAfterCharacterKeyEvent) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;

  // Activate cursor.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnCursorActivationEvent();
  // End session successfully by typing a character key.
  const KeyEvent key_event(EventType::kKeyPressed, VKEY_A, DomCode::US_A,
                           EF_NONE, DomKey::FromCharacter('a'),
                           EventTimeForNow());
  session_metrics_recorder.OnSessionEndEvent(key_event);

  histogram_tester.ExpectUniqueSample(
      kTouchCursorSessionTouchDownCountHistogramName, 1, 1);
}

TEST(TouchSelectionSessionMetricsTest, DoesNotRecordTimedOutSession) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Activate selection.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  // Time out the session.
  task_environment.FastForwardBy(base::Seconds(50));
  session_metrics_recorder.OnMenuCommand(true);

  // The session timed out and wasn't activated again, so we don't record touch
  // down count metrics.
  histogram_tester.ExpectTotalCount(
      kTouchSelectionSessionTouchDownCountHistogramName, 0);
}

TEST(TouchSelectionSessionMetricsTest, ActivationAfterTimeOut) {
  base::HistogramTester histogram_tester;
  TouchSelectionSessionMetricsRecorder session_metrics_recorder;
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Activate selection.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  // Time out the session.
  task_environment.FastForwardBy(base::Seconds(50));
  // Activate selection again.
  session_metrics_recorder.OnTouchEvent(true);
  session_metrics_recorder.OnSelectionActivationEvent();
  session_metrics_recorder.OnMenuCommand(true);

  // Metrics should be recorded for the selection session that started from the
  // activation event after the timeout.
  histogram_tester.ExpectUniqueSample(
      kTouchSelectionSessionTouchDownCountHistogramName, 1, 1);
}

}  // namespace

}  // namespace ui
