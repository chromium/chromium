# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch

from core.external_modules import numpy
from core.external_modules import pandas


SECONDS_IN_A_DAY = 60 * 60 * 24


def ResultsKey():
  """Create a DataFrame with information about result values."""
  return pandas.DataFrame.from_records([
      ('P', 'PASS', '-', 0.0),
      ('N', 'NO DATA', ' ', 0.0),
      ('X', 'SKIP', '~', 0.0),
      ('Q', 'FAIL', 'F', 1.0),
      ('Y', 'NOTRUN', '?', 0.1),
      ('L', 'FLAKY', '!', 0.5),
  ], columns=('code', 'result', 'char', 'badness'), index='code')


def FilterBy(df, **kwargs):
  """Filter out a data frame by applying patterns to columns.

  Args:
    df: A data frame to filter.
    **kwargs: Remaining keyword arguments are interpreted as column=pattern
      specifications. The pattern may contain shell-style wildcards, only rows
      whose value in the specified column matches the pattern will be kept in
      the result. If the pattern is None, no filtering is done.

  Returns:
    The filtered data frame (a view on the original data frame).
  """
  for column, pattern in kwargs.items():
    if pattern is not None:
      df = df[df[column].str.match(fnmatch.translate(pattern), case=False)]
  return df


def CompactResults(results):
  """Aggregate results from multime runs into a single result code."""
  def Compact(result):
    if len(result) == 1:
      return result  # Test ran once; use same value.
    if all(r == 'Q' for r in result):
      return 'Q'  # All runs failed; test failed.
    return 'L'  # Sometimes failed, sometimes not; test flaky.

  return results.map({r: Compact(r) for r in results.unique()})


def AggregateBuilds(df, half_life):
  """Aggregate results from multiple builds of the same test configuration.

  Also computes the "flakiness" of a test configuration.

  Args:
    df: A data frame with test results per build for a single test
      configuration (i.e. fixed master, builder, test_type).
    half_life: A number of days. Builds failures from these many days ago are
      half as important as a build failing today.
  """
  results_key = ResultsKey()
  df = df.copy()
  df['result'] = CompactResults(df['result'])
  df['status'] = df['result'].map(results_key['char'])
  df['flakiness'] = df['result'].map(results_key['badness'])
  time_ago = df['timestamp'].max() - df['timestamp']
  days_ago = time_ago.dt.total_seconds() / SECONDS_IN_A_DAY
  df['weight'] = numpy.power(0.5, days_ago / half_life)
  df['flakiness'] *= df['weight']
  latest_build = df['build_number'].iloc[0]

  grouped = df.groupby(['builder', 'test_suite', 'test_case'])
  df = grouped['flakiness'].sum().to_frame()
  df['flakiness'] *= 100 / grouped['weight'].sum()
  df['build_number'] = latest_build
  df['status'] = grouped['status'].agg(lambda s: s.str.cat())
  return df
