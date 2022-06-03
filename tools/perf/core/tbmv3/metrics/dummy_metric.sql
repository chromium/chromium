-- Copyright 2020 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

CREATE VIEW dummy_metric_output AS
SELECT DummyMetric(
  'simple_field', 42,
  'repeated_field', (
      WITH cte(col1) AS (VALUES (1), (2), (3))
      SELECT RepeatedField(col1) FROM cte
  ),
  'simple_nested', NestedMetric(
    'unannotated_field', 43,
    'annotated_field', 44
  ),
  'repeated_nested', (
    WITH cte(col1, col2) AS (VALUES (1, 2), (3, 4))
    SELECT RepeatedField(
      NestedMetric(
        'unannotated_field', col1,
        'annotated_field', col2
      )
    ) FROM cte
  )
);
