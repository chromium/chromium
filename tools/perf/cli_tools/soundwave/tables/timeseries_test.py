# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import unittest

from cli_tools.soundwave import pandas_sqlite
from cli_tools.soundwave import tables
from core.external_modules import pandas


def SamplePoint(point_id, value, timestamp=None, missing_commit_pos=False):
  """Build a sample point as returned by timeseries2 API."""
  revisions = {
      'r_commit_pos': str(point_id),
      'r_chromium': 'chromium@%d' % point_id,
  }
  annotations = {
      'a_tracing_uri': 'http://example.com/trace/%d' % point_id
  }

  if timestamp is None:
    timestamp = datetime.datetime.utcfromtimestamp(
        1234567890 + 60 * point_id).isoformat()
  if missing_commit_pos:
    # Some data points have a missing commit position.
    revisions['r_commit_pos'] = None
  return [
      point_id,
      revisions,
      value,
      timestamp,
      annotations,
  ]


class TestKey(unittest.TestCase):
  def testKeyFromDict_typical(self):
    key1 = tables.timeseries.Key.FromDict({
        'test_suite': 'loading.mobile',
        'bot': 'ChromiumPerf:android-nexus5',
        'measurement': 'timeToFirstInteractive',
        'test_case': 'Wikipedia'})
    key2 = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='Wikipedia')
    self.assertEqual(key1, key2)

  def testKeyFromDict_defaultTestCase(self):
    key1 = tables.timeseries.Key.FromDict({
        'test_suite': 'loading.mobile',
        'bot': 'ChromiumPerf:android-nexus5',
        'measurement': 'timeToFirstInteractive'})
    key2 = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='')
    self.assertEqual(key1, key2)

  def testKeyFromDict_invalidArgsRaises(self):
    with self.assertRaises(TypeError):
      tables.timeseries.Key.FromDict({
          'test_suite': 'loading.mobile',
          'bot': 'ChromiumPerf:android-nexus5'})


@unittest.skipIf(pandas is None, 'pandas not available')
class TestTimeSeries(unittest.TestCase):
  def testDataFrameFromJsonV1(self):
    test_path = ('ChromiumPerf/android-nexus5/loading.mobile'
                 '/timeToFirstInteractive/PageSet/Google')
    data = {
        'test_path': test_path,
        'improvement_direction': 1,
        'timeseries': [
            ['revision', 'value', 'timestamp', 'r_commit_pos', 'r_chromium'],
            [547397, 2300.3, '2018-04-01T14:16:32.000', '547397', 'adb123'],
            [547398, 2750.9, '2018-04-01T18:24:04.000', '547398', 'cde456'],
            [547423, 2342.2, '2018-04-02T02:19:00.000', '547423', 'fab789'],
            # Some timeseries have a missing commit position.
            [547836, 2402.5, '2018-04-02T02:20:00.000', None, 'acf147'],
        ]
    }

    timeseries = tables.timeseries.DataFrameFromJson(test_path, data)
    # Check the integrity of the index: there should be no duplicates.
    self.assertFalse(timeseries.index.duplicated().any())
    self.assertEqual(len(timeseries), 4)

    # Check values on the first point of the series.
    point = timeseries.reset_index().iloc[0]
    self.assertEqual(point['test_suite'], 'loading.mobile')
    self.assertEqual(point['measurement'], 'timeToFirstInteractive')
    self.assertEqual(point['bot'], 'ChromiumPerf/android-nexus5')
    self.assertEqual(point['test_case'], 'PageSet/Google')
    self.assertEqual(point['improvement_direction'], 'down')
    self.assertEqual(point['point_id'], 547397)
    self.assertEqual(point['value'], 2300.3)
    self.assertEqual(point['timestamp'], datetime.datetime(
        year=2018, month=4, day=1, hour=14, minute=16, second=32))
    self.assertEqual(point['commit_pos'], 547397)
    self.assertEqual(point['chromium_rev'], 'adb123')
    self.assertEqual(point['clank_rev'], None)

  def testDataFrameFromJsonV2(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='Wikipedia')
    data = {
        'improvement_direction': 'down',
        'units': 'ms',
        'data': [
            SamplePoint(547397, 2300.3, timestamp='2018-04-01T14:16:32.000'),
            SamplePoint(547398, 2750.9),
            SamplePoint(547423, 2342.2),
            SamplePoint(547836, 2402.5, missing_commit_pos=True),
        ]
    }

    timeseries = tables.timeseries.DataFrameFromJson(test_path, data)
    # Check the integrity of the index: there should be no duplicates.
    self.assertFalse(timeseries.index.duplicated().any())
    self.assertEqual(len(timeseries), 4)

    # Check values on the first point of the series.
    point = timeseries.reset_index().iloc[0]
    self.assertEqual(point['test_suite'], 'loading.mobile')
    self.assertEqual(point['measurement'], 'timeToFirstInteractive')
    self.assertEqual(point['bot'], 'ChromiumPerf:android-nexus5')
    self.assertEqual(point['test_case'], 'Wikipedia')
    self.assertEqual(point['improvement_direction'], 'down')
    self.assertEqual(point['units'], 'ms')
    self.assertEqual(point['point_id'], 547397)
    self.assertEqual(point['value'], 2300.3)
    self.assertEqual(point['timestamp'], datetime.datetime(
        year=2018, month=4, day=1, hour=14, minute=16, second=32))
    self.assertEqual(point['commit_pos'], 547397)
    self.assertEqual(point['chromium_rev'], 'chromium@547397')
    self.assertEqual(point['clank_rev'], None)

  def testDataFrameFromJson_withSummaryMetric(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='')
    data = {
        'improvement_direction': 'down',
        'units': 'ms',
        'data': [
            SamplePoint(547397, 2300.3),
            SamplePoint(547398, 2750.9),
        ],
    }

    timeseries = tables.timeseries.DataFrameFromJson(
        test_path, data).reset_index()
    self.assertTrue((timeseries['test_case'] == '').all())

  def testGetTimeSeries(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='Wikipedia')
    data = {
        'improvement_direction': 'down',
        'units': 'ms',
        'data': [
            SamplePoint(547397, 2300.3),
            SamplePoint(547398, 2750.9),
            SamplePoint(547423, 2342.2),
        ]
    }

    timeseries_in = tables.timeseries.DataFrameFromJson(test_path, data)
    with tables.DbSession(':memory:') as con:
      pandas_sqlite.InsertOrReplaceRecords(con, 'timeseries', timeseries_in)
      timeseries_out = tables.timeseries.GetTimeSeries(con, test_path)
      # Both DataFrame's should be equal, except the one we get out of the db
      # does not have an index defined.
      timeseries_in = timeseries_in.reset_index()
      self.assertTrue(timeseries_in.equals(timeseries_out))

  def testGetTimeSeries_withSummaryMetric(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='')
    data = {
        'improvement_direction': 'down',
        'units': 'ms',
        'data': [
            SamplePoint(547397, 2300.3),
            SamplePoint(547398, 2750.9),
            SamplePoint(547423, 2342.2),
        ]
    }

    timeseries_in = tables.timeseries.DataFrameFromJson(test_path, data)
    with tables.DbSession(':memory:') as con:
      pandas_sqlite.InsertOrReplaceRecords(con, 'timeseries', timeseries_in)
      timeseries_out = tables.timeseries.GetTimeSeries(con, test_path)
      # Both DataFrame's should be equal, except the one we get out of the db
      # does not have an index defined.
      timeseries_in = timeseries_in.reset_index()
      self.assertTrue(timeseries_in.equals(timeseries_out))

  def testGetMostRecentPoint_success(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='Wikipedia')
    data = {
        'improvement_direction': 'down',
        'units': 'ms',
        'data': [
            SamplePoint(547397, 2300.3),
            SamplePoint(547398, 2750.9),
            SamplePoint(547423, 2342.2),
        ]
    }

    timeseries = tables.timeseries.DataFrameFromJson(test_path, data)
    with tables.DbSession(':memory:') as con:
      pandas_sqlite.InsertOrReplaceRecords(con, 'timeseries', timeseries)
      point = tables.timeseries.GetMostRecentPoint(con, test_path)
      self.assertEqual(point['point_id'], 547423)

  def testGetMostRecentPoint_empty(self):
    test_path = tables.timeseries.Key(
        test_suite='loading.mobile',
        measurement='timeToFirstInteractive',
        bot='ChromiumPerf:android-nexus5',
        test_case='Wikipedia')

    with tables.DbSession(':memory:') as con:
      point = tables.timeseries.GetMostRecentPoint(con, test_path)
      self.assertIsNone(point)
