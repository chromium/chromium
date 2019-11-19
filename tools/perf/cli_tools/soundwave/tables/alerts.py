# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cli_tools.soundwave import pandas_sqlite


TABLE_NAME = 'alerts'
COLUMN_TYPES = (
    ('key', str),  # unique datastore key ('agxzfmNocm9tZXBlcmZyFAsS')
    ('timestamp', 'datetime64[ns]'),  # when the alert was created
    ('test_suite', str),  # benchmark name ('loading.mobile')
    ('measurement', str),  # metric name ('timeToFirstContentfulPaint')
    ('bot', str),  # master/builder name ('ChromiumPerf.android-nexus5')
    ('test_case', str),  # story name ('Wikipedia')
    ('start_revision', str),  # git hash or commit position before anomaly
    ('end_revision', str),  # git hash or commit position after anomaly
    ('median_before_anomaly', 'float64'),  # median of values before anomaly
    ('median_after_anomaly', 'float64'),  # median of values after anomaly
    ('units', str),  # unit in which values are masured ('ms')
    ('improvement', bool),  # whether anomaly is an improvement or regression
    ('bug_id', 'int64'),  # crbug id associated with this alert, 0 if missing
    ('status', str),  # one of 'ignored', 'invalid', 'triaged', 'untriaged'
    ('bisect_status', str),  # one of 'started', 'falied', 'completed'
)
COLUMNS = tuple(c for c, _ in COLUMN_TYPES)
INDEX = COLUMNS[0]


_CODE_TO_STATUS = {
    -2: 'ignored',
    -1: 'invalid',
    None: 'untriaged',
    # Any positive integer represents a bug_id and maps to a 'triaged' status.
}


def DataFrame(rows=None):
  return pandas_sqlite.DataFrame(COLUMN_TYPES, index=INDEX, rows=rows)


def _RowFromJson(data):
  """Turn json data from an alert into a tuple with values for that record."""
  data = data.copy()  # Do not modify the original dict.

  # Name fields using newer dashboard nomenclature.
  data['test_suite'] = data.pop('testsuite')
  raw_test = data.pop('test')
  if '/' in raw_test:
    data['measurement'], data['test_case'] = raw_test.split('/', 1)
  else:
    # Alert was on a summary metric, i.e. a summary of the measurement across
    # multiple test cases. Therefore, no test_case is associated with it.
    data['measurement'], data['test_case'] = raw_test, None
  data['bot'] = '/'.join([data.pop('master'), data.pop('bot')])

  # Separate bug_id from alert status.
  data['status'] = _CODE_TO_STATUS.get(data['bug_id'], 'triaged')
  if data['status'] == 'triaged':
    assert data['bug_id'] > 0
  else:
    # pandas cannot hold both int and None values in the same series, if so the
    # type is coerced into float; to prevent this we use 0 to denote untriaged
    # alerts with no bug_id assigned.
    data['bug_id'] = 0

  return tuple(data[k] for k in COLUMNS)


def DataFrameFromJson(data):
  return DataFrame([_RowFromJson(d) for d in data['anomalies']])
