-- A metric that estimates power consumption by CPU and SoC based on CPU load.
-- It requires the power_profile table to be filled with the device power
-- profile data.
-- Output values are in Amper-hours.

SELECT RUN_METRIC('android/android_proxy_power.sql');

CREATE VIEW RunStory AS
SELECT
    MIN(ts) AS begin,
    MAX(ts + dur) AS end
FROM slice
WHERE name LIKE '%.RunStory';

CREATE VIEW estimate AS
SELECT
    SUM(dur * COALESCE(power_ma, 0) / 1e9) AS power_mas
FROM power_per_thread
WHERE
    ts > (SELECT begin FROM RunStory)
    AND ts + dur < (SELECT end FROM RunStory)
    AND utid != 0;

CREATE VIEW power_cpu_estimate_output AS
SELECT PowerCpuEstimate(
  'story_power_ah',
      (SELECT power_mas / 3.6e6 FROM estimate),
  'story_duration_ms',
      (SELECT (end - begin) / 1e6 from RunStory)
);
