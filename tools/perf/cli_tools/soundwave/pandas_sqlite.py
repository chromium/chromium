# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Helper methods for dealing with a SQLite database with pandas.
"""

from core.external_modules import pandas


def DataFrame(column_types, index=None, rows=None):
  """Create a DataFrame with given column types as index.

  Unlike usual pandas DataFrame constructors, this allows to have explicitly
  typed column values, even when no rows of data are provided. And, when such
  data is available, values are explicitly casted, instead of letting pandas
  guess a type.

  Args:
    column_types: A sequence of (name, dtype) pairs to define the columns.
    index: An optional column name or sequence of column names to use as index
      of the frame.
    rows: An optional sequence of rows of data.
  """
  if rows:
    cols = zip(*rows)
    assert len(cols) == len(column_types)
    cols = (list(vs) for vs in cols)
  else:
    cols = (None for _ in column_types)
  df = pandas.DataFrame()
  for (column, dtype), values in zip(column_types, cols):
    df[column] = pandas.Series(values, dtype=dtype)
  if index is not None:
    index = [index] if isinstance(index, basestring) else list(index)
    df.set_index(index, inplace=True)
  return df


def CreateTableIfNotExists(con, name, frame):
  """Create a new empty table, if it doesn't already exist.

  Args:
    con: A sqlite connection object.
    name: Name of SQL table to create.
    frame: A DataFrame used to infer the schema of the table; the index of the
      DataFrame is set as PRIMARY KEY of the table.
  """
  keys = [k for k in frame.index.names if k is not None]
  if not keys:
    keys = None
  db = pandas.io.sql.SQLiteDatabase(con)
  table = pandas.io.sql.SQLiteTable(
      name, db, frame=frame, index=keys is not None, keys=keys,
      if_exists='append')
  table.create()


def _InsertOrReplaceStatement(name, keys):
  columns = ','.join(keys)
  values = ','.join('?' for _ in keys)
  return 'INSERT OR REPLACE INTO %s(%s) VALUES (%s)' % (name, columns, values)


def InsertOrReplaceRecords(con, name, frame):
  """Insert or replace records from a DataFrame into a SQLite database.

  Assumes that the table already exists. Any new records with a matching
  PRIMARY KEY, usually the frame.index, will replace existing records.

  Args:
    con: A sqlite connection object.
    name: Name of SQL table.
    frame: DataFrame with records to write.
  """
  db = pandas.io.sql.SQLiteDatabase(con)
  table = pandas.io.sql.SQLiteTable(
      name, db, frame=frame, index=True, if_exists='append')
  assert table.exists()
  keys, data = table.insert_data()
  insert_statement = _InsertOrReplaceStatement(name, keys)
  with db.run_transaction() as c:
    c.executemany(insert_statement, zip(*data))
