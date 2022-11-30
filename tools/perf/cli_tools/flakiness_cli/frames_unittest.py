# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import shutil
import tempfile
import unittest

import six

if six.PY2:
  import mock
else:
  import unittest.mock as mock  # pylint: disable=no-name-in-module,import-error,wrong-import-order

from cli_tools.flakiness_cli import frames
from core.external_modules import pandas


@unittest.skipIf(pandas is None, 'pandas not available')
class TestDataFrames(unittest.TestCase):
  def testBuildersDataFrame(self):
    sample_data = {
        'masters': [
            {
                'name': 'chromium.perf',
                'tests': {
                    'all_platforms_test': {
                        'builders': [
                            'my-mac-bot',
                            'my-linux-bot',
                            'my-android-bot'
                        ]
                    },
                    'desktop_test': {
                        'builders': [
                            'my-mac-bot',
                            'my-linux-bot'
                        ]
                    },
                    'mobile_test': {
                        'builders': [
                            'my-android-bot'
                        ]
                    }
                }
            },
            {
                'name': 'chromium.perf.fyi',
                'tests': {
                    'mobile_test': {
                        'builders': [
                            'my-new-android-bot'
                        ]
                    }
                }
            }
        ]
    }
    df = frames.BuildersDataFrame(sample_data)
    # Poke and check a few simple facts about our sample data.
    # There are two masters: chromium.perf, chromium.perf.fyi.
    six.assertCountEqual(self, df['master'].unique(),
                         ['chromium.perf', 'chromium.perf.fyi'])
    # The 'desktop_test' runs on desktop builders only.
    six.assertCountEqual(
        self, df[df['test_type'] == 'desktop_test']['builder'].unique(),
        ['my-mac-bot', 'my-linux-bot'])
    # The 'mobile_test' runs on mobile builders only.
    six.assertCountEqual(
        self, df[df['test_type'] == 'mobile_test']['builder'].unique(),
        ['my-android-bot', 'my-new-android-bot'])
    # The new android bot is on the chromium.perf.fyi waterfall.
    six.assertCountEqual(
        self, df[df['builder'] == 'my-new-android-bot']['master'].unique(),
        ['chromium.perf.fyi'])

  def testRunLengthDecode(self):
    encoded = [[3, 'F'], [4, 'P'], [2, 'F']]
    decoded = ['F', 'F', 'F', 'P', 'P', 'P', 'P', 'F', 'F']

    # pylint: disable=protected-access
    self.assertSequenceEqual(
        list(frames._RunLengthDecode(encoded)), decoded)

  def testIterTestResults(self):
    tests_dict = {
        'A': {
            '1': {'results': 'A/1'},
            '2': {'results': 'A/2'}
        },
        'B': {
            '3': {
                'a': {'results': 'B/3/a'},
                'b': {'results': 'B/3/b'},
                'c': {'results': 'B/3/c'}
            }
        },
        'C': {'results': 'C'}
    }

    expected = [
        ('A', '1', {'results': 'A/1'}),
        ('A', '2', {'results': 'A/2'}),
        ('B', '3/a', {'results': 'B/3/a'}),
        ('B', '3/b', {'results': 'B/3/b'}),
        ('B', '3/c', {'results': 'B/3/c'}),
        ('C', '', {'results': 'C'}),
    ]

    # pylint: disable=protected-access
    six.assertCountEqual(self, list(frames._IterTestResults(tests_dict)),
                         expected)

  def testTestResultsDataFrame(self):
    data = {
        'android-bot': {
            'secondsSinceEpoch': [1234567892, 1234567891, 1234567890],
            'buildNumbers': [42, 41, 40],
            'chromeRevision': [1234, 1233, 1232],
            'tests': {
                'some_benchmark': {
                    'story_1': {
                        'results': [[3, 'P']],
                        'times': [[3, 1]]
                    },
                    'story_2': {
                        'results': [[2, 'Q']],
                        'times': [[2, 5]]
                    }
                },
                'another_benchmark': {
                    'story_3': {
                        'results': [[1, 'Q'], [2, 'P']],
                        'times': [[1, 3], [1, 2], [1, 3]]
                    }
                }
            }
        },
        'version': 4
    }
    df = frames.TestResultsDataFrame(data)
    # Poke and check a few simple facts about our sample data.
    # We have data from 3 builds x 3 stories:
    self.assertEqual(len(df), 9)
    # All run on the same bot.
    self.assertTrue((df['builder'] == 'android-bot').all())
    # The most recent build number was 42.
    self.assertEqual(df['build_number'].max(), 42)
    # some_benchmark/story_1 passed on all builds.
    selection = df[df['test_case'] == 'story_1']
    self.assertTrue((selection['test_suite'] == 'some_benchmark').all())
    self.assertTrue((selection['result'] == 'P').all())
    # There was no data for story_2 on build 40.
    selection = df[(df['test_case'] == 'story_2') & (df['build_number'] == 40)]
    self.assertEqual(len(selection), 1)
    self.assertTrue(selection.iloc[0]['result'], 'N')

  def testTestResultsDataFrame_empty(self):
    data = {
        'android-bot': {
            'secondsSinceEpoch': [1234567892, 1234567891, 1234567890],
            'buildNumbers': [42, 41, 40],
            'chromeRevision': [1234, 1233, 1232],
            'tests': {}
        },
        'version': 4
    }
    df = frames.TestResultsDataFrame(data)
    # The data frame is empty.
    self.assertTrue(df.empty)
    # Column names are still defined (although of course empty).
    six.assertCountEqual(self, df['test_case'].unique(), [])

  def testTestResultsDataFrame_wrongVersionRejected(self):
    data = {
        'android-bot': {
            'some': ['new', 'fancy', 'results', 'encoding']
        },
        'version': 5
    }
    with self.assertRaises(AssertionError):
      frames.TestResultsDataFrame(data)

  def testGetWithCache(self):
    def make_frame_1():
      # test_2 was failing.
      return frames.pandas.DataFrame.from_records(
          [['test_1', 'P'], ['test_2', 'Q']], columns=('test_name', 'result'))

    def make_frame_2():
      # test_2 is now passing.
      return frames.pandas.DataFrame.from_records(
          [['test_1', 'P'], ['test_2', 'P']], columns=('test_name', 'result'))

    def make_frame_fail():
      self.fail('make_frame should not be called')

    expected_1 = make_frame_1()
    expected_2 = make_frame_2()
    filename = 'example_frame.pkl'
    one_hour = datetime.timedelta(hours=1)
    temp_dir = tempfile.mkdtemp()
    try:
      with mock.patch.object(
          frames, 'CACHE_DIR', os.path.join(temp_dir, 'cache')):
        # Cache is empty, so the frame is created from our function.
        df = frames.GetWithCache(filename, make_frame_1, one_hour)
        self.assertTrue(df.equals(expected_1))

        # On the second try, the frame can be retrieved from cache; the
        # make_frame function should not be called.
        df = frames.GetWithCache(filename, make_frame_fail, one_hour)
        self.assertTrue(df.equals(expected_1))

        # Pretend two hours have elapsed, we should now get a new data frame.
        last_update = datetime.datetime(2018, 8, 24, 15)
        pretend_now = datetime.datetime(2018, 8, 24, 17)
        with mock.patch.object(datetime, 'datetime') as dt:
          dt.utcfromtimestamp.return_value = last_update
          dt.utcnow.return_value = pretend_now
          df = frames.GetWithCache(filename, make_frame_2, one_hour)
        self.assertFalse(df.equals(expected_1))
        self.assertTrue(df.equals(expected_2))
    finally:
      shutil.rmtree(temp_dir)
