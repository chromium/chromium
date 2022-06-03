-- Copyright 2020 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

-- First create a view that exposes cpu time of all slices.
-- TODO(dproy): Extract this view into a common helper file.
CREATE VIEW cpu_slices
AS
SELECT
  slice.id,
  slice.name AS slice_name,
  slice.ts,
  slice.dur,
  slice.depth AS depth,
  thread_counter_track.id AS counter_track_id,
  (
    SELECT max(value)
    FROM counter
    WHERE ts = slice.ts AND track_id = thread_counter_track.id
  ) AS cpu_start,
  (
    SELECT max(value)
    FROM counter
    WHERE ts = (slice.ts + slice.dur) AND track_id = thread_counter_track.id
  ) AS cpu_end
FROM slice
INNER JOIN thread_track ON slice.track_id = thread_track.id
INNER JOIN thread_counter_track ON thread_track.utid = thread_counter_track.utid
WHERE slice.dur >= 0;

-- Sum over the cpu time of top level slices (slices with depth 0).
CREATE VIEW total_cpu_time
AS SELECT sum(cpu_end - cpu_start) AS total_cpu FROM cpu_slices WHERE depth = 0;

CREATE VIEW cpu_time_metric
AS
SELECT
  cast((SELECT total_cpu FROM total_cpu_time) AS float)
  / (SELECT (end_ts - start_ts) FROM trace_bounds) AS cpu_time_percentage;

CREATE VIEW cpu_time_metric_output
AS
SELECT CpuTimeMetric('cpu_time_percentage', cpu_time_percentage)
FROM cpu_time_metric;
