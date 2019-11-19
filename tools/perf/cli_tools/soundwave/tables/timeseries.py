# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from cli_tools.soundwave import pandas_sqlite
from core.external_modules import pandas


TABLE_NAME = 'timeseries'
COLUMN_TYPES = (
    # Index columns.
    ('test_suite', str),  # benchmark name ('loading.mobile')
    ('measurement', str),  # metric name ('timeToFirstContentfulPaint')
    ('bot', str),  # master/builder name ('ChromiumPerf.android-nexus5')
    ('test_case', str),  # story name ('Wikipedia')
    ('point_id', 'int64'),  # monotonically increasing id for time series axis
    # Other columns.
    ('value', 'float64'),  # value recorded for test_path at given point_id
    ('timestamp', 'datetime64[ns]'),  # when the value got stored on dashboard
    ('commit_pos', 'int64'),  # chromium commit position
    ('chromium_rev', str),  # git hash of chromium revision
    ('clank_rev', str),  # git hash of clank revision
    ('trace_url', str),  # URL to a sample trace.
    ('units', str),  # unit of measurement (e.g. 'ms', 'bytes')
    ('improvement_direction', str),  # good direction ('up', 'down', 'unknown')
)
COLUMNS = tuple(c for c, _ in COLUMN_TYPES)
INDEX = COLUMNS[:5]

# Copied from https://goo.gl/DzGYpW.
_CODE_TO_IMPROVEMENT_DIRECTION = {
    0: 'up',
    1: 'down',
}


TEST_PATH_PARTS = (
    'master', 'builder', 'test_suite', 'measurement', 'test_case')

# Query template to find all data points of a given test_path (i.e. fixed
# test_suite, measurement, bot, and test_case values).
_QUERY_TIME_SERIES = (
    'SELECT * FROM %s WHERE %s'
    % (TABLE_NAME, ' AND '.join('%s=?' % c for c in INDEX[:-1])))


# Required columns to request from /timeseries2 API.
_TIMESERIES2_COLS = [
    'revision',
    'revisions',
    'avg',
    'timestamp',
    'annotations']


class Key(collections.namedtuple('Key', INDEX[:-1])):
  """Uniquely identifies a single timeseries."""

  @classmethod
  def FromDict(cls, *args, **kwargs):
    kwargs = dict(*args, **kwargs)
    kwargs.setdefault('test_case', '')  # test_case is optional.
    return cls(**kwargs)

  def AsDict(self):
    return dict(zip(self._fields, self))

  def AsApiParams(self):
    """Return a dict with params for a /timeseries2 API request."""
    params = self.AsDict()
    if not params['test_case']:
      del params['test_case']  # test_case is optional.
    params['columns'] = ','.join(_TIMESERIES2_COLS)
    return params


def DataFrame(rows=None):
  return pandas_sqlite.DataFrame(COLUMN_TYPES, index=INDEX, rows=rows)


def _ParseIntValue(value, on_error=-1):
  # Try to parse as int and, in case of error, return a pre-defined value.
  try:
    return int(value)
  except StandardError:
    return on_error


def _ParseConfigFromTestPath(test_path):
  if isinstance(test_path, Key):
    return test_path.AsDict()

  values = test_path.split('/', len(TEST_PATH_PARTS) - 1)
  if len(values) < len(TEST_PATH_PARTS):
    values.append('')  # Possibly missing test_case.
  if len(values) != len(TEST_PATH_PARTS):
    raise ValueError(test_path)
  config = dict(zip(TEST_PATH_PARTS, values))
  config['bot'] = '%s/%s' % (config.pop('master'), config.pop('builder'))
  return config


def DataFrameFromJson(test_path, data):
  if isinstance(test_path, Key):
    return _DataFrameFromJsonV2(test_path, data)
  else:
    # TODO(crbug.com/907121): Remove when we can switch entirely to v2.
    return _DataFrameFromJsonV1(test_path, data)


def _DataFrameFromJsonV2(ts_key, data):
  rows = []
  for point in data['data']:
    point = dict(zip(_TIMESERIES2_COLS, point))
    rows.append(ts_key + (
        point['revision'],  # point_id
        point['avg'],  # value
        point['timestamp'],  # timestamp
        _ParseIntValue(point['revisions']['r_commit_pos']),  # commit_pos
        point['revisions'].get('r_chromium'),  # chromium_rev
        point['revisions'].get('r_clank'),  # clank_rev
        point['annotations'].get('a_tracing_uri'),  # trace_url
        data['units'],  # units
        data['improvement_direction'],  # improvement_direction
    ))
  return DataFrame(rows)


def _DataFrameFromJsonV1(test_path, data):
  # The dashboard API returns an empty list if there is no recent data for the
  # timeseries.
  if not data:
    return DataFrame()
  assert test_path == data['test_path']
  config = _ParseConfigFromTestPath(data['test_path'])
  config['improvement_direction'] = _CODE_TO_IMPROVEMENT_DIRECTION.get(
      data['improvement_direction'], 'unknown')
  timeseries = data['timeseries']
  # The first element in timeseries list contains header with column names.
  header = timeseries[0]
  rows = []

  # Remaining elements contain the values for each row.
  for values in timeseries[1:]:
    row = config.copy()
    row.update(zip(header, values))
    row['point_id'] = row['revision']
    row['commit_pos'] = _ParseIntValue(row['r_commit_pos'])
    row['chromium_rev'] = row.get('r_chromium')
    row['clank_rev'] = row.get('r_clank', None)
    rows.append(tuple(row.get(k) for k in COLUMNS))

  return DataFrame(rows)


def GetTimeSeries(con, test_path, extra_cond=None):
  """Get the records for all data points on the given test_path.

  Returns:
    A pandas.DataFrame with all records found.
  """
  config = _ParseConfigFromTestPath(test_path)
  params = tuple(config[c] for c in INDEX[:-1])
  query = _QUERY_TIME_SERIES
  if extra_cond is not None:
    query = ' '.join([query, extra_cond])
  return pandas.read_sql(query, con, params=params, parse_dates=['timestamp'])


def GetMostRecentPoint(con, test_path):
  """Find the record for the most recent data point on the given test_path.

  Returns:
    A pandas.Series with the record if found, or None otherwise.
  """
  df = GetTimeSeries(con, test_path, 'ORDER BY timestamp DESC LIMIT 1')
  return df.iloc[0] if not df.empty else None
