# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.services import dashboard_service
from cli_tools.soundwave.tables import timeseries


CLOUD_PATH = 'gs://chrome-health-tvdata/datasets/v8_report.csv'

PIXEL_32_BITS = 'ChromiumPerf:android-pixel2-perf'
PIXEL_64_BITS = 'ChromiumPerfFyi:android-pixel2-perf-fyi'

BROWSING_TEST_SUITES = {
    'v8.browsing_mobile': ['Total:duration', 'V8-Only:duration']
}

PRESS_BENCHMARKS = [
    {
        'test_suite': 'speedometer2',
        'measurement': 'RunsPerMinute',
        'test_case': 'Speedometer2'
    },
    {
        'test_suite': 'octane',
        'measurement': 'Total.Score',
    },
    {
        'test_suite': 'jetstream',
        'measurement': 'Score',
    }
]


def GetV8BrowsingMobile():
  # The describe API doesn't currently work with v8.browsing_mobile, so use
  # system_health.memory_mobile instead since it has the same test cases.
  description = dashboard_service.Describe('system_health.memory_mobile')
  return description['cases']


def IterTestPaths():
  v8_browsing_test_cases = GetV8BrowsingMobile()

  for bot in [PIXEL_32_BITS, PIXEL_64_BITS]:
    bot_path = bot.replace(':', "/")
    for test_suite, measurements in BROWSING_TEST_SUITES.iteritems():
      # v8.browsing_mobile only runs 'browse:*' stories, while other benchmarks
      # run all of them.
      browse_only = 'browsing' in test_suite
      for test_case in v8_browsing_test_cases:
        if browse_only and not test_case.startswith('browse:'):
          continue

        # Don't yield the page category entries.
        test_case_parts = test_case.split(':')
        if len(test_case_parts) <= 2:
          continue

        # Remove disabled tests from the list of those to be collected.
        if test_case_parts[2] in ['toi', 'globo', 'flipkart', 'avito']:
          continue

        page = '_'.join(test_case_parts)
        page_category = '_'.join(test_case_parts[0:2])

        for measurement in measurements:
          # The v2 API doesn't support v8.browsing_mobile, so fall back on the
          # v1 API for now.
          yield '/'.join(
              [bot_path, test_suite, measurement, page_category, page])

    for series in PRESS_BENCHMARKS:
      yield timeseries.Key.FromDict(series, bot=bot)
