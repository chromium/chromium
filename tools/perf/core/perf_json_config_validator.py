# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import json

from chrome_telemetry_build import android_browser_types
from core import path_util
from core import bot_platforms

_VALID_SWARMING_DIMENSIONS = {
    'gpu', 'device_ids', 'os', 'pool', 'perf_tests', 'perf_tests_with_args',
    'cpu', 'device_os', 'device_status', 'device_type', 'device_os_flavor',
    'id', 'mac_model', 'synthetic_product_name'
}
_DEFAULT_VALID_PERF_POOLS = {
    'chrome.tests.perf',
    'chrome.tests.perf-pgo',
    'chrome.tests.perf-webview',
    'chrome.tests.perf-fyi',
    'chrome.tests.perf-webview-fyi',
}
_VALID_PERF_POOLS = {
    'android-builder-perf': {'chrome.tests'},
    'android_arm64-builder-perf': {'chrome.tests'},
    'chromeos-kevin-perf-fyi': {'chrome.tests'},
    'fuchsia-perf-nsn': {'chrome.tests'},
    'fuchsia-perf-shk': {'chrome.tests'},
    'linux-builder-perf': {'chrome.tests'},
    'mac-arm-builder-perf': {'chrome.tests'},
    'mac-arm-builder-perf-pgo': {'chrome.tests'},
    'mac-builder-perf': {'chrome.tests'},
    'win64-builder-perf': {'chrome.tests'},
}
_VALID_WEBVIEW_BROWSERS = {
    'android-webview',
    'android-webview-google',
    'android-webview-trichrome-google-bundle',
}

_PERFORMANCE_TEST_SUITES = {
    'performance_test_suite',
    'performance_test_suite_eve',
    'performance_test_suite_octopus',
    'performance_webview_test_suite',
}
for suffix in android_browser_types.TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES:
  _PERFORMANCE_TEST_SUITES.add('performance_test_suite' + suffix)


def _ValidateSwarmingDimension(builder_name, swarming_dimensions):
  for dimension in swarming_dimensions:
    for k, v in dimension.items():
      if k not in _VALID_SWARMING_DIMENSIONS:
        raise ValueError('Invalid swarming dimension in %s: %s' % (
            builder_name, k))
      if k == 'pool' and v not in _VALID_PERF_POOLS.get(
          builder_name, _DEFAULT_VALID_PERF_POOLS):
        raise ValueError('Invalid perf pool %s in %s' % (v, builder_name))
      if k == 'os' and v == 'Android':
        if (not 'device_type' in dimension.keys() or
            not 'device_os_flavor' in dimension.keys()):
          raise ValueError(
              'Invalid android dimensions %s in %s' % (v, builder_name))


def _ParseShardMapFileName(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--test-shard-map-filename', dest='shard_file')
  options, _ = parser.parse_known_args(args)
  return options.shard_file


def _ParseBrowserFlags(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--browser')
  parser.add_argument('--webview-embedder-apk', action='append')
  options, _ = parser.parse_known_args(args)
  return options


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')


def _ValidateShardingData(builder_name, test_config):
  num_shards = test_config['swarming'].get('shards', 1)
  if num_shards == 1:
    return
  shard_file_name = _ParseShardMapFileName(test_config['args'])
  if not shard_file_name:
    raise ValueError('Must specify the shard map for case num shard >= 2')
  shard_file_path = os.path.join(_SHARD_MAP_DIR, shard_file_name)
  if not os.path.exists(shard_file_path):
    raise ValueError(
        "shard test file %s in config of builder %s does not exist" % (
          repr(shard_file_name), repr(builder_name)))

  with open(shard_file_path) as f:
    shard_map_data = json.load(f)

  shard_map_data.pop('extra_infos', None)
  shard_keys = set(shard_map_data.keys())
  expected_shard_keys = {str(i) for i in range(num_shards)}
  if shard_keys != expected_shard_keys:
    raise ValueError(
        'The shard configuration of %s does not match the expected expected '
        'number of shards (%d) in config of builder %s' % (
            repr(shard_file_name), num_shards, repr(builder_name)))


def _ValidateBrowserType(builder_name, test_config):
  browser_options = _ParseBrowserFlags(test_config['args'])
  if 'WebView' in builder_name or 'webview' in builder_name:
    if browser_options.browser not in _VALID_WEBVIEW_BROWSERS:
      raise ValueError('%s must use one of the following browsers: %s' %
                       (builder_name, ', '.join(_VALID_WEBVIEW_BROWSERS)))
  elif 'Android' in builder_name or 'android' in builder_name:
    android_browsers = ('android-chromium', 'android-chrome',
                        'android-chrome-bundle', 'android-chrome-64-bundle',
                        'android-trichrome-chrome-google-64-32-bundle',
                        'android-trichrome-bundle', 'exact')
    if browser_options.browser not in android_browsers:
      raise ValueError( 'The browser type for %s must be one of %s' % (
          builder_name, ', '.join(android_browsers)))
  elif 'chromeos' in builder_name:
    if browser_options.browser != 'cros-chrome':
      raise ValueError("%s must use 'cros-chrome' browser type" %
                       builder_name)
  elif 'lacros' in builder_name:
    if browser_options.browser != 'lacros-chrome':
      raise ValueError("%s must use 'lacros-chrome' browser type" %
                       builder_name)
  elif builder_name in ('win-10-perf', 'win-10-perf-pgo',
                        'win-11-perf', 'win-11-perf-pgo',
                        'Win 7 Nvidia GPU Perf',
                        'win-10_laptop_low_end-perf_HP-Candidate',
                        'win-10_laptop_low_end-perf',
                        'win-10_laptop_low_end-perf-pgo',
                        'win-10_amd_laptop-perf', 'win-10_amd_laptop-perf-pgo'):
    if browser_options.browser != 'release_x64':
      raise ValueError("%s must use 'release_x64' browser type" %
                       builder_name)
  else:  # The rest must be desktop/laptop builders
    if browser_options.browser != 'release':
      raise ValueError("%s must use 'release' browser type" %
                       builder_name)


def ValidateTestingBuilder(builder_name, builder_data):
  isolated_scripts = builder_data['isolated_scripts']
  test_names = []
  for test_config in isolated_scripts:
    test_names.append(test_config['name'])
    _ValidateSwarmingDimension(
        builder_name,
        swarming_dimensions=test_config['swarming'].get('dimension_sets', {}))
    if test_config['test'] in _PERFORMANCE_TEST_SUITES:
      _ValidateShardingData(builder_name, test_config)
      _ValidateBrowserType(builder_name, test_config)

  if any(suite in test_names for suite in _PERFORMANCE_TEST_SUITES):
    if test_names[-1] not in _PERFORMANCE_TEST_SUITES:
      raise ValueError(
          'performance_test_suite-based targets must run at the end of builder '
          '%s to avoid starving other test step (see crbug.com/873389). '
          'Instead found %s' % (repr(builder_name), test_names[-1]))



def _IsBuilderName(name):
  return not name.startswith('AAA')


def _IsTestingBuilder(builder_name, builder_data):
  del builder_name  # unused
  return 'isolated_scripts' in builder_data


def ValidatePerfConfigFile(file_handle, is_main_perf_waterfall):
  perf_data = json.load(file_handle)
  perf_testing_builder_names = set()
  for key, value in perf_data.items():
    if not _IsBuilderName(key):
      continue
    if _IsTestingBuilder(builder_name=key, builder_data=value):
      ValidateTestingBuilder(builder_name=key, builder_data=value)
      try:
        trigger_script = value['isolated_scripts'][-1]['trigger_script'][
            'script']
      except KeyError:
        continue
      if trigger_script ==  '//testing/trigger_scripts/perf_device_trigger.py':
        perf_testing_builder_names.add(key)
  if (is_main_perf_waterfall and
      perf_testing_builder_names != bot_platforms.OFFICIAL_PLATFORM_NAMES):
    raise ValueError(
        'Found mismatches between actual perf waterfall builders and platforms '
        'in core.bot_platforms. Please update the platforms in '
        'bot_platforms.py.\nPlatforms should be added to core.bot_platforms:%s'
        '\nPlatforms should be removed from core.bot_platforms:%s' % (
          perf_testing_builder_names - bot_platforms.OFFICIAL_PLATFORM_NAMES,
          bot_platforms.OFFICIAL_PLATFORM_NAMES - perf_testing_builder_names))


def main(args):
  del args  # unused
  waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.json')
  fyi_waterfall_file = os.path.join(
      path_util.GetChromiumSrcDir(), 'testing', 'buildbot',
      'chromium.perf.fyi.json')
  calibration_waterfall_file = os.path.join(path_util.GetChromiumSrcDir(),
                                            'testing', 'buildbot',
                                            'chromium.perf.calibration.json')

  with open(fyi_waterfall_file) as f:
    ValidatePerfConfigFile(f, False)

  with open(waterfall_file) as f:
    ValidatePerfConfigFile(f, True)

  with open(calibration_waterfall_file) as f:
    ValidatePerfConfigFile(f, False)
