# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cli_tools.soundwave import pandas_sqlite
from core.external_modules import pandas


TABLE_NAME = 'bugs'
COLUMN_TYPES = (
    ('id', 'int64'),  # crbug number identifying this issue
    ('summary', unicode),  # issue title ('1%-5% regression in loading ...')
    ('published', 'datetime64[ns]'),  # when the issue got created
    ('updated', 'datetime64[ns]'),  # when the issue got last updated
    ('state', str),  # usually either 'open' or 'closed'
    ('status', str),  # current state of the bug ('Assigned', 'Fixed', etc.)
    ('author', str),  # email of user who created the issue
    ('owner', str),  # email of user who currently owns the issue
    ('cc', str),  # comma-separated list of users cc'ed into the issue
    ('components', str),  # comma-separated list of components ('Blink>Loader')
    ('labels', str),  # comma-separated list of labels ('Type-Bug-Regression')
)
COLUMNS = tuple(c for c, _ in COLUMN_TYPES)
DATE_COLUMNS = tuple(c for c, t in COLUMN_TYPES if t == 'datetime64[ns]')
INDEX = COLUMNS[0]


def DataFrame(rows=None):
  return pandas_sqlite.DataFrame(COLUMN_TYPES, index=INDEX, rows=rows)


def _CommaSeparate(values):
  assert isinstance(values, list)
  if values:
    return ','.join(values)
  else:
    return None


def DataFrameFromJson(data):
  rows = []
  for row in data:
    row = row['bug'].copy()
    for key in ('cc', 'components', 'labels'):
      row[key] = _CommaSeparate(row[key])
    rows.append(tuple(row[k] for k in COLUMNS))

  return DataFrame(rows)


def Get(con, bug_id):
  """Find the record for a bug_id in the given database connection.

  Returns:
    A pandas.Series with the record if found, or None otherwise.
  """
  df = pandas.read_sql(
      'SELECT * FROM %s WHERE id=?' % TABLE_NAME, con, params=(bug_id,),
      index_col=INDEX, parse_dates=DATE_COLUMNS)
  return df.loc[bug_id] if len(df) else None
