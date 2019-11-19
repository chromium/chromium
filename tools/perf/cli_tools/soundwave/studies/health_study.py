# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.services import dashboard_service
from cli_tools.soundwave.tables import timeseries


CLOUD_PATH = 'gs://chrome-health-tvdata/datasets/health_study.csv'

SYSTEM_HEALTH = [
    {
        'test_suite': 'system_health.memory_mobile',
        'measurement': ('memory:{browser}:all_processes:reported_by_os:'
                        'private_footprint_size'),
    },
    {
        'test_suite': 'system_health.common_mobile',
        'measurement': 'cpu_time_percentage'
    }
]

STARTUP_BY_BROWSER = {
    'chrome': {
        'test_suite': 'startup.mobile',
        'measurement': 'first_contentful_paint_time',
        'test_case': 'intent_coldish_bbc'
    },
    'webview': {
        'test_suite': 'system_health.webview_startup',
        'measurement': 'webview_startup_wall_time_avg',
        'test_case': 'load:chrome:blank'
    }
}

APK_SIZE = {
    'test_suite': 'resource_sizes:Monochrome.minimal.apks',
    'measurement': 'Specifics:normalized apk size',
    'bot': 'ChromiumPerf:android-builder-perf',
}


def IterSystemHealthBots():
  yield 'ChromiumPerf:android-go-perf'
  yield 'ChromiumPerf:android-go_webview-perf'
  yield 'ChromiumPerf:android-pixel2-perf'
  yield 'ChromiumPerf:android-pixel2_webview-perf'


def GetBrowserFromBot(bot):
  return 'webview' if 'webview' in bot else 'chrome'


def GetHealthCheckStories():
  description = dashboard_service.Describe('system_health.common_mobile')
  return description['caseTags']['health_check']


def IterTestPaths():
  test_cases = GetHealthCheckStories()
  for bot in IterSystemHealthBots():
    browser = GetBrowserFromBot(bot)

    # Startup.
    yield timeseries.Key.FromDict(STARTUP_BY_BROWSER[browser], bot=bot)

    # Memory.
    if bot == 'ChromiumPerf:android-pixel2_webview-perf':
      # The pixel2 webview bot incorrectly reports memory as if coming from
      # chrome. TODO(crbug.com/972620): Remove this when bug is fixed.
      browser = 'chrome'

    for series in SYSTEM_HEALTH:
      measurement = series['measurement'].format(browser=browser)
      for test_case in test_cases:
        yield timeseries.Key.FromDict(
            series, bot=bot, measurement=measurement, test_case=test_case)

  # APK size.
  yield timeseries.Key.FromDict(APK_SIZE)
