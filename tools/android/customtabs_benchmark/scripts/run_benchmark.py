#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Loops Custom Tabs tests and outputs the results into a CSV file."""

import copy
import json
import logging
import optparse
import os
import sys
import threading

import customtabs_benchmark

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium


_KEYS = ['url', 'warmup', 'skip_launcher_activity', 'speculation_mode',
         'delay_to_may_launch_url', 'delay_to_launch_url', 'cold',
         'pinning_benchmark', 'extra_brief_memory_mb', 'pin_filename',
         'pin_offset', 'pin_length']


def _ParseConfiguration(filename):
  """Reads a JSON file and returns a list of configurations.

  Each valid value in the JSON file can be either a scalar or a list of
  values. This function expands the scalar values to be lists. All list must
  have the same length.

  Sample configuration:
  {
    "url": "https://www.android.com",
    "warmup": [false, true],
    "skip_launcher_activity": true,
    "speculation_mode": "speculative_prefetch",
    "delay_to_may_launch_url": [-1, 1000],
    "delay_to_launch_url": [-1, 1000],
    "cold": true
  }
  See sample_config.json in this directory as well.

  Args:
    filename: (str) Point to a file containins a JSON dictionnary of config
              values.

  Returns:
    A list of configurations, where each value is specified.
  """
  config = json.load(open(filename, 'r'))
  has_all_values = all(k in config for k in _KEYS)
  assert has_all_values
  config['url'] = str(config['url'])  # Intents don't like unicode.
  has_list = any(isinstance(config[k], list) for k in _KEYS)
  if not has_list:
    return [config]
  list_keys = [k for k in _KEYS if isinstance(config[k], list)]
  list_length = len(config[list_keys[0]])
  assert all(len(config[k]) == list_length for k in list_keys)
  result = []
  for i in range(list_length):
    result.append(copy.deepcopy(config))
    for k in list_keys:
      result[-1][k] = result[-1][k][i]
  return result


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Loops tests on all attached '
                                 'devices, with randomly selected '
                                 'configurations, and outputs the results in '
                                 'CSV files.')
  parser.add_option('--config', help='JSON configuration file. Required.')
  parser.add_option('--output_file_prefix', help='Output file prefix. Actual '
                    'output file is prefix_<device ID>.csv', default='result')
  parser.add_option('--once', help='Run only once.', default=False,
                    action='store_true')
  parser.add_option('--adb_path', help='Path to ADB', default=None)
  return parser


def _Run(output_file_prefix, configs):
  """Loops the tests described by the configs on connected devices.

  Args:
    output_file_prefix: (str) Prefix for the output file name.
    configs: ([dict]) List of configurations.
  """
  devices = device_utils.DeviceUtils.HealthyDevices()
  stop_event = threading.Event()
  threads = []
  for device in devices:
    output_filename = '%s_%s.csv' % (output_file_prefix, str(device))
    thread = threading.Thread(
        target=customtabs_benchmark.LoopOnDevice,
        args=(device, configs, output_filename),
        kwargs={'should_stop': stop_event})
    thread.start()
    threads.append(thread)
  while any(thread.is_alive() for thread in threads):
    try:
      for thread in threads:
        if thread.is_alive():
          thread.join(1.)
    except KeyboardInterrupt as _:
      logging.warning('Stopping now.')
      stop_event.set()


def main():
  parser = _CreateOptionParser()
  options, _ = parser.parse_args()
  if options.config is None:
    logging.error('A configuration file must be provided.')
    sys.exit(0)
  devil_chromium.Initialize(adb_path=options.adb_path)
  configs = _ParseConfiguration(options.config)
  if options.once:
    device = device_utils.DeviceUtils.HealthyDevices()[0]
    customtabs_benchmark.LoopOnDevice(device, [configs[0]], '-', once=True)
  else:
    _Run(options.output_file_prefix, configs)


if __name__ == '__main__':
  main()
