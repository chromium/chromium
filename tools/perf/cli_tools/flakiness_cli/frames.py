# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to convert json responses from test-results into data frames."""

import datetime
import os

from core.external_modules import pandas


CACHE_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '_cached_data', 'flakiness_cli'))

TEST_RESULTS_COLUMNS = (
    'timestamp', 'builder', 'build_number', 'commit_pos', 'test_suite',
    'test_case', 'result', 'time')


def BuildersDataFrame(data):
  """Convert a builders request response into a data frame."""
  def iter_rows():
    for master in data['masters']:
      for test_type, builders in master['tests'].items():
        for builder_name in builders['builders']:
          yield master['name'], builder_name, test_type

  return pandas.DataFrame.from_records(
      iter_rows(), columns=('master', 'builder', 'test_type'))


def _RunLengthDecode(count_value_pairs):
  """Iterator to expand a run length encoded sequence.

  The test results dashboard compresses some long lists using "run length
  encoding", for example:

    ['F', 'F', 'F', 'P', 'P', 'P', 'P', 'F', 'F']

  becomes:

    [[3, 'F'], [4, 'P'], [2, 'F']]

  This function takes the encoded version and returns an iterator that yields
  the elements of the expanded one.

  Args:
    count_value_pairs: A list of [count, value] pairs.

  Yields:
    Each value of the expanded sequence, one at a time.
  """
  for count, value in count_value_pairs:
    for _ in range(count):
      yield value


def _IterTestResults(tests_dict, test_path=None):
  """Parse and iterate over the "tests" section of a test results response.

  The test results dashboard supports multiple levels of "subtests" organised
  as a tree of nested dicts. The leafs of the tree are "results" dicts.

  This iterator "flattens out" the test path names, splitting them into a
  test_suite (the top level name) and test_case (all other "sub" names, if any).

  For example:

      {
          'A': {
              '1': {'results': [...]},
              '2': {'results': [...]}
          },
          'B': {
              '3': {
                  'a': {'results': [...]},
                  'b': {'results': [...]},
                  'c': {'results': [...]}
              }
          },
          'C': {'results': [...]}
      }

  Will generate 6 responses when iterated over:

      ('A', '1', {'results': [...]})
      ('A', '2', {'results': [...]})
      ('B', '3/a', {'results': [...]})
      ('B', '3/b', {'results': [...]})
      ('B', '3/c', {'results': [...]})
      ('C', '', {'results': [...]})

  Args:
    tests_dict: The 'tests' dictionary as contained in a test results response.
    test_path: An optional prefix for the test path. (Not meant to be used
        directly, needed for the recursive implementation.)

  Yields:
    A tripe (test_suite, test_case, test_results) for each test path contained
    in the input.
  """
  if 'results' in tests_dict:
    assert test_path  # Should not be missing or empty.
    yield test_path[0], '/'.join(test_path[1:]), tests_dict
  else:
    if test_path is None:
      test_path = []
    for test_name, subtests_dict in tests_dict.items():
      test_path.append(test_name)
      for test_row in _IterTestResults(subtests_dict, test_path):
        yield test_row
      assert test_path.pop() == test_name


def _AddDataFrameColumn(df, col, values, fill_value=0):
  """Add a sequence of values as a new column, filling values if needed.

  Args:
    df: A data frame on which to add the column, it is modified in place.
    col: A string with the name for the new column.
    values: A sequence of values for the column.
    fill_value: If the sequence of values is shorter that the current number
      of rows in the df, pad the sequence with extra copies of `fill_value`
      to make the number of rows match.
  """
  df[col] = pandas.Series(list(values)).reindex(df.index, fill_value=fill_value)


def TestResultsDataFrame(data):
  """Convert a test results request response into a data frame."""
  assert data['version'] == 4

  dfs = []
  for builder, builder_data in data.items():
    if builder == 'version':
      continue  # Skip, not a builder.
    builds = pandas.DataFrame()
    builds['timestamp'] = pandas.to_datetime(
        builder_data['secondsSinceEpoch'], unit='s')
    builds['builder'] = builder
    builds['build_number'] = builder_data['buildNumbers']
    _AddDataFrameColumn(builds, 'commit_pos', builder_data['chromeRevision'])
    for test_suite, test_case, test_results in _IterTestResults(
        builder_data['tests']):
      df = builds.copy()
      df['test_suite'] = test_suite
      df['test_case'] = test_case
      _AddDataFrameColumn(df, 'result', _RunLengthDecode(
          test_results['results']), fill_value='N')
      _AddDataFrameColumn(df, 'time', _RunLengthDecode(test_results['times']))
      dfs.append(df)

    if dfs:
      df = pandas.concat(dfs, ignore_index=True)
      assert tuple(df.columns) == TEST_RESULTS_COLUMNS
    else:
      # Return an empty data frame with the right column names otherwise.
      df = pandas.DataFrame(columns=TEST_RESULTS_COLUMNS)

    return df


def GetWithCache(filename, frame_maker, expires_after):
  """Get a data frame from cache or, if necessary, create and cache it.

  Args:
    filename: The name of a file for the cached copy of the data frame,
      it will be stored in the CACHE_DIR.
    frame_maker: A function that takes no arguments and returns a data frame,
      only called to create the data frame if the cached copy does not exist
      or is too old.
    expires_after: A datetime.timedelta object, the cached copy will not be
      used if it was created longer that this time ago.
  """
  filepath = os.path.join(CACHE_DIR, filename)
  try:
    timestamp = os.path.getmtime(filepath)
    last_modified = datetime.datetime.utcfromtimestamp(timestamp)
    expired = datetime.datetime.utcnow() > last_modified + expires_after
  except OSError:  # If the file does not exist.
    expired = True

  if expired:
    df = frame_maker()
    if not os.path.exists(CACHE_DIR):
      os.makedirs(CACHE_DIR)
    df.to_pickle(filepath)
  else:
    df = pandas.read_pickle(filepath)
  return df
