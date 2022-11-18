-- A metric that collects on-device power rails measurement for the
-- duration of the story run. Power drain breakdown is device-specific
-- since different devices have different sensors.
-- See go/power-mobile-benchmark for the list of supported devices.
-- Output values are in Joules (Watt-seconds).

SELECT RUN_METRIC('android/power_drain_in_watts.sql');

CREATE VIEW run_story_event AS
SELECT ts, dur
FROM slice
WHERE name LIKE '%.RunStory';

CREATE VIRTUAL TABLE run_story_span_join_drain
USING SPAN_JOIN(run_story_event, drain_in_watts);

-- Compute cumulative power drain over the duration of the run_story event.
CREATE VIEW story_drain AS
WITH
  drain_per_rail AS (
    SELECT
      name,
      sum(dur * drain_w / 1e9) AS drain_j,
      sum(dur / 1e6) AS dur_ms
    FROM run_story_span_join_drain
    GROUP BY name
  )
SELECT subsystem, sum(drain_j) AS drain_j, max(dur_ms) AS dur_ms
FROM drain_per_rail
JOIN power_counters
  USING (name)
GROUP BY subsystem;


CREATE VIEW interaction_events AS
SELECT ts, dur
FROM slice
WHERE name  LIKE 'Interaction.%';

CREATE VIRTUAL TABLE interactions_span_join_drain
USING SPAN_JOIN(interaction_events, drain_in_watts);

-- Compute cumulative power drain over the total duration of interaction events.
CREATE VIEW interaction_drain AS
WITH
  drain_per_rail AS (
    SELECT
      name,
      sum(dur * drain_w / 1e9) AS drain_j,
      sum(dur / 1e6) AS dur_ms
    FROM interactions_span_join_drain
    GROUP BY name
  )
SELECT subsystem, sum(drain_j) AS drain_j, max(dur_ms) AS dur_ms
FROM drain_per_rail
JOIN power_counters
  USING (name)
GROUP BY subsystem;

-- Output power consumption as measured by several ODPMs, over the following
-- time frames:
-- story_* values - over the duration of the story run. This doesn't include
-- Chrome starting and loading a page.
-- interaction_* values - over the combined duration of Interaction.* events,
-- e.g. Interaction.Gesture_ScrollAction.
CREATE VIEW power_rails_metric_output AS
SELECT PowerRailsMetric(
  'story_total_j',
      (SELECT sum(drain_j) FROM story_drain),
  'story_cpu_big_core_cluster_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'cpu_big'),
  'story_cpu_mid_core_cluster_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'cpu_mid'),
  'story_cpu_little_core_cluster_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'cpu_little'),
  'story_soc_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'soc'),
  'story_mem_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'mem'),
  'story_gpu_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'gpu'),
  'story_cellular_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'cellular'),
  'story_wifi_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'wifi'),
  'story_display_j',
      (SELECT drain_j FROM story_drain WHERE subsystem = 'display'),
  'story_cpu_and_system_j',
      (SELECT sum(drain_j) FROM story_drain WHERE
          subsystem IN ('cpu_big', 'cpu_mid', 'cpu_little', 'soc')),
  'story_duration_ms',
      (SELECT dur_ms FROM story_drain WHERE subsystem = 'display'),
  'interaction_total_j',
      (SELECT sum(drain_j) FROM interaction_drain),
  'interaction_cpu_big_core_cluster_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'cpu_big'),
  'interaction_cpu_mid_core_cluster_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'cpu_mid'),
  'interaction_cpu_little_core_cluster_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'cpu_little'),
  'interaction_soc_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'soc'),
  'interaction_mem_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'mem'),
  'interaction_gpu_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'gpu'),
  'interaction_cellular_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'cellular'),
  'interaction_wifi_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'wifi'),
  'interaction_display_j',
      (SELECT drain_j FROM interaction_drain WHERE subsystem = 'display'),
  'interaction_cpu_and_system_j',
      (SELECT sum(drain_j) FROM interaction_drain WHERE
          subsystem IN ('cpu_big', 'cpu_mid', 'cpu_little', 'soc')),
  'interaction_duration_ms',
      (SELECT dur_ms FROM interaction_drain WHERE subsystem = 'display')
);
