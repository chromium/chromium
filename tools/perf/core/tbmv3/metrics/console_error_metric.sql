-- Copyright 2019 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

CREATE VIEW console_error_events AS
SELECT args.string_value as source
from slices
inner join args using(arg_set_id)
where slices.name = "ConsoleMessage::Error"
  and slices.category = "blink.console"
  and args.flat_key = "debug.source"
UNION ALL
SELECT "JS" AS source
FROM slices
WHERE slices.category = 'v8.console' AND (
  slices.name = 'V8ConsoleMessage::Exception' OR
  slices.name = 'V8ConsoleMessage::Error' OR
  slices.name = 'V8ConsoleMessage::Assert'
);

CREATE VIEW console_error_metric AS
SELECT
  (SELECT COUNT(*) FROM console_error_events) as all_errors,
  (SELECT COUNT(*) FROM console_error_events where source = "JS") as js_errors,
  (SELECT COUNT(*) FROM console_error_events where source = "Network") as network_errors;

CREATE VIEW console_error_metric_output AS
SELECT ConsoleErrorMetric(
  'all_errors', all_errors,
  'js_errors', js_errors,
  'network_errors', network_errors)
FROM console_error_metric
