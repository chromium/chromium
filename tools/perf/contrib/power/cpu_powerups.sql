-- Find causes for CPUs powering up.
--
-- Copyright 2022 The Chromium Authors
--
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- The scripts below analyse traces with the following tracing options
-- enabled:
--
--  - Linux kernel:
---    "power/*", "sched/*", "task/*",
--  - Chromium:
--      "toplevel", "toplevel.flow".

-- Noteworthy tables:
--
--   cpu_power_first_top_level_slice_after_powerup :: The top-level slices
--      that ran after a CPU power-up.

-- The CPU power transitions in the trace.
--
-- Schema:
--   ts          : The timestamp at the start of the slice.
--   dur         : The duration of the slice.
--   cpu         : The CPU on which the transition occurred
--   power_state : The power state that the CPU was in at time 'ts' for
--                 duration 'dur'.
--   previous_power_state : The power state that the CPU was previously in.
--   powerup_id  : A unique ID for the CPU power-up.
--
-- Power states are encoded as non-negative integers, with zero representing
-- full-power operation and positive values representing increasingly deep
-- sleep states.
--
-- On ARM systems, power state 1 represents the WFI (Wait For Interrupt) sleep
-- state that the CPU enters while idle.
DROP VIEW IF EXISTS cpu_power_slice;
CREATE VIEW cpu_power_slice AS
  WITH cpu_power_states AS (
    SELECT
      c.id AS id,
      cct.cpu AS cpu,
      c.ts AS ts,
      -- Encode the 'value' field as a power state.
      CAST((CASE c.value WHEN 4294967295 THEN 0 ELSE c.value + 1 END)
        AS INT) AS power_state
    FROM counter AS c
    JOIN cpu_counter_track AS cct
      ON c.track_id = cct.id
    WHERE cct.name = 'cpuidle'
  )
  SELECT *
  FROM (
    SELECT
      -- Fields named 'ts' and 'dur' are needed for span joins.
      ts,
      LEAD(ts) OVER (PARTITION BY cpu ORDER BY ts ASC) - ts
        AS dur,
      LEAD(ts) OVER (PARTITION BY cpu ORDER BY ts ASC) - ts
        AS power_duration,
      cpu,
      power_state,
      LAG(power_state) OVER (PARTITION BY cpu ORDER BY ts ASC)
        AS previous_power_state,
      id AS powerup_id
    FROM cpu_power_states
  )
  WHERE dur IS NOT NULL
    AND previous_power_state IS NOT NULL
    AND power_state = 0                      -- Track full-power states.
    AND power_state != previous_power_state  -- Skip missing spans.
    ORDER BY ts ASC;

-- We do not want scheduler slices with utid = 0 (the 'swapper' kernel thread).
DROP VIEW IF EXISTS cpu_power_valid_sched_slice;
CREATE VIEW cpu_power_valid_sched_slice AS
  SELECT *
  FROM sched_slice
  WHERE utid != 0;

-- Join scheduler slices with the spans with CPU power slices.
--
-- There multiple scheduler slices could fall into one CPU power slice.
--
---  CPU Power:
--   |----------------------------|....................|---------|
--   A       <cpu active>         B     <cpu idling>   C         D

--   Scheduler slices on that CPU:
--     |-----T1-----| |....T2....|                      |---T3--|
--     E            F G          H                      I       J
--
-- Here threads T1 and T2 executed in CPU power slice [A,B].  The
-- time between F and G represents time between threads in the kernel.
DROP TABLE IF EXISTS cpu_power_and_sched_slice;
CREATE VIRTUAL TABLE cpu_power_and_sched_slice
USING
  SPAN_JOIN(cpu_power_slice PARTITIONED cpu,
            cpu_power_valid_sched_slice PARTITIONED cpu);

-- The Linux scheduler slices that executed immediately after a
-- CPU power up.
--
-- Schema:
--   ts         : The timestamp at the start of the slice.
--   dur        : The duration of the slice.
--   cpu        : The cpu on which the slice executed.
--   sched_id   : Id for the sched_slice table.
--   utid       : Unique id for the thread that ran within the slice.
--   previous_power_state : The CPU's power state before this slice.
--   powerup_id : The id of the power-up slice.
--   power_duration : The duration of the underlying power slice.
DROP VIEW IF EXISTS cpu_power_first_sched_slice_after_powerup;
CREATE VIEW cpu_power_first_sched_slice_after_powerup AS
  SELECT
    ts,
    dur,
    cpu,
    id,
    utid,
    previous_power_state,
    powerup_id,
    power_duration
  FROM cpu_power_and_sched_slice
  WHERE power_state = 0     -- Power-ups only.
  GROUP BY cpu, powerup_id
  HAVING ts = MIN(ts)       -- There will only be one MIN sched slice
                            -- per CPU power up.
  ORDER BY ts ASC;

-- A view with counts of power-ups grouped by Linux process & thread.
--
-- Schema:
--   process_name   : The Linux process that ran after a power up.
--   thread_name    : The thread in the Linux process that powered up.
--   powerup_count  : The counts for the (process, thread) pair.
DROP VIEW IF EXISTS cpu_power_powerup_count_by_process_and_thread;
CREATE VIEW cpu_power_powerup_count_by_process_and_thread AS
  SELECT
    process.name AS process_name,
    thread.name AS thread_name,
    count() AS powerup_count
  FROM cpu_power_first_sched_slice_after_powerup
  JOIN thread using (utid)
  JOIN process using (upid)
  GROUP BY process_name, thread_name
  ORDER BY powerup_count DESC;

-- A view joining thread tracks and top-level slices.
--
-- This view is intended to be intersected by time with the scheduler
-- slices scheduled after a CPU power up.
--
-- Schema:
--   utid     : Thread unique id.
--   slice_id : The slice_id for the top-level slice.
--   ts       : Starting timestamp for the slice.
--   dur      : The duration for the slice.
DROP VIEW IF EXISTS cpu_power_thread_and_toplevel_slice;
CREATE VIEW cpu_power_thread_and_toplevel_slice AS
  SELECT t.utid, s.id AS slice_id, s.ts AS ts, s.dur AS dur
  FROM slice AS s
  JOIN thread_track AS t
    ON s.track_id = t.id
  WHERE s.depth = 0   -- Top-level slices only.
  ORDER BY ts ASC;

-- A table holding the slices that executed within the scheduler
-- slice that ran on a CPU immediately after power-up.
--
-- Schema:
--   ts       : Timestamp of the resulting slice
--   dur      : Duration of the slice.
--   cpu      : The CPU the sched slice ran on.
--   utid     : Unique thread id for the slice.
--   sched_id : 'id' field from the sched_slice table.
--   type     : From the sched_slice table, always 'sched_slice'.
--   end_state : The ending state for the sched_slice
--   priority : The kernel thread priority
--   slice_id : Id of the top-level slice for this (sched) slice.
DROP TABLE IF EXISTS cpu_power_post_powerup_slice;
CREATE VIRTUAL TABLE cpu_power_post_powerup_slice
USING
  SPAN_JOIN(cpu_power_first_sched_slice_after_powerup PARTITIONED utid,
            cpu_power_thread_and_toplevel_slice PARTITIONED utid);

-- The first top-level slice that ran after a CPU power-up.
DROP VIEW IF EXISTS cpu_power_first_top_level_slice_after_powerup;
CREATE VIEW cpu_power_first_top_level_slice_after_powerup AS
  SELECT slice_id, previous_power_state, power_duration
  FROM cpu_power_post_powerup_slice
  GROUP BY cpu, powerup_id
  HAVING ts = MIN(ts)
  ORDER BY ts ASC;

--
-- Chrome-specific analyses.
--

-- Bring in the Chrome tasks table.
SELECT RUN_METRIC("chrome/chrome_tasks.sql");

-- Rename fields of the 'chrome_tasks' table, so that a subsequent
-- 'span_join' can work.
DROP VIEW IF EXISTS chrome_tasks_view_with_renamed_fields;
CREATE VIEW chrome_tasks_view_with_renamed_fields AS
  SELECT
    ts,
    dur,
    slice_id,
    name AS slice_name,
    full_name,
    task_type,
    utid,
    thread_name,
    process_name,
    upid
  FROM chrome_tasks;

-- A table with Chrome-related slices that executed within the scheduler
-- slice that ran on a CPU after a power-up.
--
-- Schema:
--   ts           : Timestamp of the slice.
--   dur          : Duration of the slice.
--   slice_id     : The ID of the Chrome slice in the 'slice' table.
--   slice_name   : The name of the slice.
--   full_name    : The complete name of the Chrome task that ran in the slice.
--   task_type    : The type of Chrome task that ran in the slice.
--   utid         : The thread in which this slice executed.
--   thread_name  : The name of the thread in which this slice executed.
--   upid         : The process in which the slice executed.
--   process_name : The name of the process in which this slice executed.
DROP TABLE IF EXISTS cpu_power_chrome_slices_after_powerup;
CREATE VIRTUAL TABLE cpu_power_chrome_slices_after_powerup
USING
  SPAN_JOIN(cpu_power_first_sched_slice_after_powerup PARTITIONED utid,
            chrome_tasks_view_with_renamed_fields PARTITIONED utid);

-- The first Chrome slice that executed after a CPU power-up.
DROP VIEW IF EXISTS cpu_power_first_chrome_slice_after_powerup;
CREATE VIEW cpu_power_first_chrome_slice_after_powerup AS
  SELECT
    *
  FROM cpu_power_chrome_slices_after_powerup
  GROUP BY cpu, powerup_id
  HAVING ts = MIN(ts)
  ORDER BY ts ASC;

SELECT "cpu_powerups"; -- Keep the Perfetto trace processor's |.read| happy.
