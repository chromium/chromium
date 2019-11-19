# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import logging
try:
  import sqlite3
except ImportError:
  pass

from core import cli_utils
from core.external_modules import pandas
from core.services import dashboard_service
from cli_tools.soundwave import pandas_sqlite
from cli_tools.soundwave import studies
from cli_tools.soundwave import tables
from cli_tools.soundwave import worker_pool


def _FetchBugsWorker(args):
  con = sqlite3.connect(args.database_file, timeout=10)

  def Process(bug_id):
    bugs = tables.bugs.DataFrameFromJson([dashboard_service.Bugs(bug_id)])
    pandas_sqlite.InsertOrReplaceRecords(con, 'bugs', bugs)

  worker_pool.Process = Process


def FetchAlertsData(args):
  params = {
      'test_suite': args.benchmark,
      'min_timestamp': cli_utils.DaysAgoToTimestamp(args.days)
  }
  if args.sheriff != 'all':
    params['sheriff'] = args.sheriff

  with tables.DbSession(args.database_file) as con:
    # Get alerts.
    num_alerts = 0
    bug_ids = set()
    # TODO: This loop may be slow when fetching thousands of alerts, needs a
    # better progress indicator.
    for data in dashboard_service.IterAlerts(**params):
      alerts = tables.alerts.DataFrameFromJson(data)
      pandas_sqlite.InsertOrReplaceRecords(con, 'alerts', alerts)
      num_alerts += len(alerts)
      bug_ids.update(alerts['bug_id'].unique())
    print('%d alerts found!' % num_alerts)

    # Get set of bugs associated with those alerts.
    bug_ids.discard(0)  # A bug_id of 0 means untriaged.
    print('%d bugs found!' % len(bug_ids))

    # Filter out bugs already in cache.
    if args.use_cache:
      known_bugs = set(
          b for b in bug_ids if tables.bugs.Get(con, b) is not None)
      if known_bugs:
        print('(skipping %d bugs already in the database)' % len(known_bugs))
        bug_ids.difference_update(known_bugs)

  # Use worker pool to fetch bug data.
  total_seconds = worker_pool.Run(
      'Fetching data of %d bugs: ' % len(bug_ids),
      _FetchBugsWorker, args, bug_ids)
  print('[%.1f bugs per second]' % (len(bug_ids) / total_seconds))


def _IterStaleTestPaths(con, test_paths):
  """Iterate over test_paths yielding only those with stale or absent data.

  A test_path is considered to be stale if the most recent data point we have
  for it in the db is more than a day older.
  """
  a_day_ago = pandas.Timestamp.utcnow() - pandas.Timedelta(days=1)
  a_day_ago = a_day_ago.tz_convert(tz=None)

  for test_path in test_paths:
    latest = tables.timeseries.GetMostRecentPoint(con, test_path)
    if latest is None or latest['timestamp'] < a_day_ago:
      yield test_path


def _FetchTimeseriesWorker(args):
  con = sqlite3.connect(args.database_file, timeout=10)
  min_timestamp = cli_utils.DaysAgoToTimestamp(args.days)

  def Process(test_path):
    try:
      if isinstance(test_path, tables.timeseries.Key):
        params = test_path.AsApiParams()
        params['min_timestamp'] = min_timestamp
        data = dashboard_service.Timeseries2(**params)
      else:
        data = dashboard_service.Timeseries(test_path, days=args.days)
    except KeyError:
      logging.info('Timeseries not found: %s', test_path)
      return

    timeseries = tables.timeseries.DataFrameFromJson(test_path, data)
    pandas_sqlite.InsertOrReplaceRecords(con, 'timeseries', timeseries)

  worker_pool.Process = Process


def _ReadTimeseriesFromFile(filename):
  with open(filename, 'r') as f:
    data = json.load(f)
  return [tables.timeseries.Key.FromDict(ts) for ts in data]


def FetchTimeseriesData(args):
  def _MatchesAllFilters(test_path):
    return all(f in test_path for f in args.filters)

  with tables.DbSession(args.database_file) as con:
    # Get test_paths.
    if args.benchmark is not None:
      test_paths = dashboard_service.ListTestPaths(
          args.benchmark, sheriff=args.sheriff)
    elif args.input_file is not None:
      test_paths = _ReadTimeseriesFromFile(args.input_file)
    elif args.study is not None:
      test_paths = list(args.study.IterTestPaths())
    else:
      raise ValueError('No source for test paths specified')

    # Apply --filter's to test_paths.
    if args.filters:
      test_paths = filter(_MatchesAllFilters, test_paths)
    num_found = len(test_paths)
    print('%d test paths found!' % num_found)

    # Filter out test_paths already in cache.
    if args.use_cache:
      test_paths = list(_IterStaleTestPaths(con, test_paths))
      num_skipped = num_found - len(test_paths)
      if num_skipped:
        print('(skipping %d test paths already in the database)' % num_skipped)

  # Use worker pool to fetch test path data.
  total_seconds = worker_pool.Run(
      'Fetching data of %d timeseries: ' % len(test_paths),
      _FetchTimeseriesWorker, args, test_paths)
  print('[%.1f test paths per second]' % (len(test_paths) / total_seconds))

  if args.output_csv is not None:
    print()
    print('Post-processing data for study ...')
    dfs = []
    with tables.DbSession(args.database_file) as con:
      for test_path in test_paths:
        df = tables.timeseries.GetTimeSeries(con, test_path)
        dfs.append(df)
    df = studies.PostProcess(pandas.concat(dfs, ignore_index=True))
    with cli_utils.OpenWrite(args.output_csv) as f:
      df.to_csv(f, index=False)
    print('Wrote timeseries data to:', args.output_csv)
