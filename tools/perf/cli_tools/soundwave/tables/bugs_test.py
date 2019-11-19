# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import unittest

from cli_tools.soundwave import tables
from core.external_modules import pandas


@unittest.skipIf(pandas is None, 'pandas not available')
class TestBugs(unittest.TestCase):
  def testDataFrameFromJson(self):
    data = [
        {
            'bug': {
                'id': 12345,
                'summary': u'1%-\u221e% regression in loading at 123:125',
                'published': '2018-04-09T17:01:09',
                'updated': '2018-04-12T06:38:34',
                'state': 'closed',
                'status': 'Fixed',
                'author': 'foo@chromium.org',
                'owner': 'bar@chromium.org',
                'cc': ['baz@chromium.org', 'foo@chromium.org'],
                'components': [],
                'labels': ['Perf-Regression', 'Foo>Label'],
            }
        }
    ]

    bugs = tables.bugs.DataFrameFromJson(data)
    self.assertEqual(len(bugs), 1)

    bug = bugs.loc[12345]  # Get bug by id.
    self.assertEqual(bug['published'], datetime.datetime(
        year=2018, month=4, day=9, hour=17, minute=1, second=9))
    self.assertEqual(bug['status'], 'Fixed')
    self.assertEqual(bug['cc'], 'baz@chromium.org,foo@chromium.org')
    self.assertEqual(bug['components'], None)

  def testDataFrameFromJson_noBugs(self):
    data = []
    bugs = tables.bugs.DataFrameFromJson(data)
    self.assertEqual(len(bugs), 0)
