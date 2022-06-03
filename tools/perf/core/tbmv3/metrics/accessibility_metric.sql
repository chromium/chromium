-- Copyright 2020 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

CREATE VIEW browser_accessibility_events
AS SELECT slice.dur / 1e6 as time
FROM slice
INNER JOIN thread_track ON slice.track_id = thread_track.id
INNER JOIN thread USING(utid)
WHERE slice.name = 'BrowserAccessibilityManager::OnAccessibilityEvents'
AND thread.name = 'CrBrowserMain';

CREATE VIEW renderer_main_thread_slices
AS SELECT slice.dur / 1e6 as time, slice.name as slice_name
FROM slice
INNER JOIN thread_track ON slice.track_id = thread_track.id
INNER JOIN thread USING(utid)
WHERE thread.name = 'CrRendererMain';

CREATE VIEW accessibility_metric_output AS
SELECT AccessibilityMetric(
  'browser_accessibility_events', (
    SELECT RepeatedField(time)
    FROM browser_accessibility_events
  ),
  'render_accessibility_events', (
    SELECT RepeatedField(time)
    FROM renderer_main_thread_slices
    WHERE slice_name = 'RenderAccessibilityImpl::SendPendingAccessibilityEvents'
  ),
  'render_accessibility_locations', (
    SELECT RepeatedField(time)
    FROM renderer_main_thread_slices
    WHERE slice_name = 'RenderAccessibilityImpl::SendLocationChanges'
  )
);
