# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from core.services import dashboard_service
from cli_tools.soundwave.tables import timeseries


CLOUD_PATH = 'gs://chrome-health-tvdata/datasets/weblayer_study.csv'

SYSTEM_HEALTH = [
    {
        'test_suite': 'system_health.memory_mobile',
        'measurement': ('memory:chrome:all_processes:reported_by_os:'
                        'private_footprint_size_avg'),
    }
]

STARTUP_BY_BROWSER = {
    'cct': {
        'test_suite': 'startup.mobile',
        'measurement': 'first_contentful_paint_time',
        'test_case': 'cct_coldish_bbc'
    },
    'weblayer': {
        'test_suite': 'startup.mobile',
        'measurement': 'first_contentful_paint_time',
        'test_case': 'intent_coldish_bbc'
    },
}


def IterSystemHealthBots():
  yield 'ChromiumPerf:android-pixel2-perf'
  yield 'ChromiumPerf:android-pixel2_weblayer-perf'


def GetBrowserFromBot(bot):
  return 'weblayer' if 'weblayer' in bot else 'cct'


def GetHealthCheckStories():
  description = dashboard_service.Describe('system_health.common_mobile')
  return description['caseTags']['health_check']


def IterTestPaths():
  test_cases = GetHealthCheckStories()
  for bot in IterSystemHealthBots():
    browser = GetBrowserFromBot(bot)

    # Startup.
    yield timeseries.Key.FromDict(STARTUP_BY_BROWSER[browser], bot=bot)

    for series in SYSTEM_HEALTH:
      measurement = series['measurement']
      for test_case in test_cases:
        print(timeseries.Key.FromDict(
            series, bot=bot, measurement=measurement, test_case=test_case))
        yield timeseries.Key.FromDict(
            series, bot=bot, measurement=measurement, test_case=test_case)
