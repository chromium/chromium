# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import six

from cli_tools.flakiness_cli import analysis
from cli_tools.flakiness_cli import frames
from core.external_modules import pandas


@unittest.skipIf(pandas is None, 'pandas not available')
class TestAnalysis(unittest.TestCase):
  def testFilterBy(self):
    builders = frames.pandas.DataFrame.from_records([
      ['chromium.perf', 'my-mac-bot', 'common_tests'],
      ['chromium.perf', 'my-linux-bot', 'common_tests'],
      ['chromium.perf', 'my-android-bot', 'common_tests'],
      ['chromium.perf', 'my-mac-bot', 'desktop_tests'],
      ['chromium.perf', 'my-linux-bot', 'desktop_tests'],
      ['chromium.perf', 'my-android-bot', 'mobile_tests'],
      ['chromium.perf.fyi', 'my-new-android-bot', 'common_tests'],
      ['chromium.perf.fyi', 'my-new-android-bot', 'mobile_tests'],
    ], columns=('master', 'builder', 'test_type'))

    # Let's find where desktop_tests are running.
    df = analysis.FilterBy(builders, test_type='desktop_tests')
    six.assertCountEqual(self, df['builder'], ['my-mac-bot', 'my-linux-bot'])

    # Let's find all android bots and tests. (`None` patterns are ignored.)
    df = analysis.FilterBy(builders, builder='*android*', master=None)
    self.assertEqual(len(df), 4)
    six.assertCountEqual(self, df['builder'].unique(),
                         ['my-android-bot', 'my-new-android-bot'])
    six.assertCountEqual(self, df['master'].unique(),
                         ['chromium.perf', 'chromium.perf.fyi'])
    six.assertCountEqual(self, df['test_type'].unique(),
                         ['common_tests', 'mobile_tests'])

    # Let's find all bots running common_tests on the main waterfall.
    df = analysis.FilterBy(
        builders, master='chromium.perf', test_type='common_tests')
    six.assertCountEqual(self, df['builder'],
                         ['my-mac-bot', 'my-linux-bot', 'my-android-bot'])

    # There are no android bots running desktop tests.
    df = analysis.FilterBy(
        builders, master='*android*', test_type='desktop_tests')
    self.assertTrue(df.empty)
