# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os

try:
  import sqlite3
except ImportError:
  pass

from cli_tools.soundwave import pandas_sqlite
from cli_tools.soundwave.tables import alerts
from cli_tools.soundwave.tables import bugs
from cli_tools.soundwave.tables import timeseries


@contextlib.contextmanager
def DbSession(filename):
  """Context manage a session with a database connection.

  Ensures that tables have been initialized.
  """
  if filename != ':memory:':
    parent_dir = os.path.dirname(filename)
    if not os.path.exists(parent_dir):
      os.makedirs(parent_dir)
  con = sqlite3.connect(filename)
  try:
    # Tell sqlite to use a write-ahead log, which drastically increases its
    # concurrency capabilities. This helps prevent 'database is locked'
    # exceptions when we have many workers writing to a single database. This
    # mode is sticky, so we only need to set it once and future connections
    # will automatically use the log. More details are available at
    # https://www.sqlite.org/wal.html.
    con.execute('PRAGMA journal_mode=WAL')
    _CreateTablesIfNeeded(con)
    yield con
  finally:
    con.close()


def _CreateTablesIfNeeded(con):
  """Creates soundwave tables in the database, if they don't already exist."""
  for m in (alerts, bugs, timeseries):
    pandas_sqlite.CreateTableIfNotExists(con, m.TABLE_NAME, m.DataFrame())
