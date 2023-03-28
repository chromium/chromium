#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Replays a touch scenario in a loop for A/B comparisons.

Usage:

* Add to out/AndroidReleaseOfficial/args.gn:
  use_jank_tracker_for_experiments = true

* Build the ReleaseOfficial flavour of Chrome
  autoninja -C out/AndroidReleaseOfficial monochrome_64_bundle

* Install it on device
  out/AndroidReleaseOfficial/bin/monochrome_64_bundle install

* Build the touch_replay
  autoninja -C out/AndroidReleaseOfficial touch_replay

* Record the scenario
  adb root
  adb push out/AndroidReleaseOfficial/touch_replay /data/local/tmp
  adb shell '/data/local/tmp/touch_replay record \
      /data/local/tmp/touch_events.dump'

* Fetch the events from the device
  adb pull '/data/local/tmp/touch_events.dump'

* Loop infinitely
  tools/android/touch_replay_benchmark/run_benchmark.py \
      --config=tools/android/touch_replay_benchmark/example.yaml \
      --output=/tmp/output \
      --events=touch_events.dump \
      --replayer=out/AndroidReleaseOfficial/touch_replay
"""

import argparse
import copy
import json
import logging
import os
import random
import re
import signal
import subprocess
import sys
import time

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party'))
import pyyaml

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_errors
from devil.android import device_utils
from devil.android import flag_changer
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium
from pylib import constants

sys.path.append(os.path.join(_SRC_PATH, 'tools', 'variations'))
import fieldtrial_util

_FIELDTRIAL_TESTING_CONFIG = os.path.join(_SRC_PATH, 'testing', 'variations',
                                          'fieldtrial_testing_config.json')

_EVENTS_FILE_ON_DEVICE = '/data/local/tmp/touch_events.dump'
_REPLAY_EXECUTABLE_ON_DEVICE = '/data/local/tmp/touch_replay'
_NOTIFY_FILE_ON_DEVICE = '/data/local/tmp/inotify-jank-report'
_EMPTY_FILE_ON_DEVICE = '/data/local/tmp/empty_file'


def _CreateParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('-d', '--device', help='Android device to use.')
  parser.add_argument('-o',
                      '--output',
                      required=True,
                      help='Directory for results.')
  parser.add_argument('-c',
                      '--config',
                      required=True,
                      help='YAML configuration file defining ' +
                      'combinations of parameters to be compared. ' +
                      'See example.yaml.')
  parser.add_argument('-e',
                      '--events',
                      required=True,
                      help='Replay file to simulate touch gestures')
  parser.add_argument('-r',
                      '--replayer',
                      required=True,
                      help='Path to the |touch_replay| binary')
  parser.add_argument('-n',
                      '--n',
                      type=int,
                      default=-1,
                      help='Number of runs to make. ' +
                      'A negative number means infinite. ' +
                      'This is the default')
  return parser


def _GetPreferredDevice(preferred_device_serial):
  if preferred_device_serial:
    devices = device_utils.DeviceUtils.HealthyDevices(
        device_arg=preferred_device_serial)
  else:
    devices = device_utils.DeviceUtils.HealthyDevices()
  if devices and devices[0].IsOnline():
    return devices[0]
  return None


def _PrepareNotification(device: device_utils.DeviceUtils):
  device.RunShellCommand(['/system/bin/rm', '-rf', _NOTIFY_FILE_ON_DEVICE],
                         check_return=True)
  device.RunShellCommand(['/system/bin/mkdir', '-p', _NOTIFY_FILE_ON_DEVICE],
                         check_return=True)


def _LogcatMessage(device, message: str):
  device.RunShellCommand(['log', '-p', 'v', '-t', 'touch_replay', message],
                         check_return=True)


def _TouchNotificationFileOnDevice(device: device_utils.DeviceUtils):
  _LogcatMessage(device, 'RequestJankTrackerCSV')
  device.RunShellCommand(['/system/bin/touch', _EMPTY_FILE_ON_DEVICE],
                         check_return=True)
  device.RunShellCommand(
      ['/system/bin/mv', '-f', _EMPTY_FILE_ON_DEVICE, _NOTIFY_FILE_ON_DEVICE],
      check_return=True)


def _ReplayTouchEvents(device: device_utils.DeviceUtils):
  time.sleep(1)
  device.RunShellCommand(
      [_REPLAY_EXECUTABLE_ON_DEVICE, 'replay', _EVENTS_FILE_ON_DEVICE],
      as_root=True,
      check_return=True)
  time.sleep(5)
  _TouchNotificationFileOnDevice(device)


def _ReplayTouchEventsWithCpuProfile(device: device_utils.DeviceUtils,
                                     trace_dir: str, config):
  os.makedirs(trace_dir)
  command = [
      config['cpu_profile'], '--config', config['chrometto_config'], '-o',
      trace_dir
  ]
  proc = subprocess.Popen(command)

  # Replay.
  _ReplayTouchEvents(device)

  # Collect the trace.
  proc_pid = proc.pid
  os.kill(proc_pid, signal.SIGINT)
  os.waitpid(proc_pid, 0)


def _ExtractJankCsv(logcat_monitor):
  try:
    result_line_re = re.compile(r'JankyDurationTrackerCSV:(.*)$')
    match = logcat_monitor.WaitFor(result_line_re, timeout=60)
    occurrences = sum(
        1 for _ in logcat_monitor.FindAll(r'.*JankyDurationTrackerCSV:.*'))
    if occurrences != 1:
      logging.error(
          'JankyDurationTrackerCSV occurrences: {}, skipping result'.format(
              occurrences))
    else:
      return match[1]
  except device_errors.CommandTimeoutError as _:
    logging.warning('Timeout waiting for the result line')
  return None


def _FlattenParsedConfig(config: dict):
  expanded_keys = ['args', 'url', 'enable_chrometto_tracing']
  list_keys = [k for k in expanded_keys if isinstance(config[k], list)]
  if len(list_keys) == 0:
    return [config]
  list_length = len(config[list_keys[0]])
  assert all(len(config[k]) == list_length for k in list_keys)
  result = []
  for i in range(list_length):
    result.append(copy.deepcopy(config))
    for key in list_keys:
      result[-1][key] = result[-1][key][i]
  return result


def _FlagsForConfig(config):
  fieldtrial_args = fieldtrial_util.GenerateArgs(_FIELDTRIAL_TESTING_CONFIG,
                                                 'android')
  flags = fieldtrial_args + [
      '--disable-fre',
      '--watch-dir-for-scroll-jank-report={}'.format(_NOTIFY_FILE_ON_DEVICE)
  ]
  config_args = config['args']
  if 'named_args' in config and config_args in config['named_args']:
    config_args = config['named_args'][config_args]
  flags.extend(config_args.split())
  if config.get('enable_chrometto_tracing', False):
    flags.append('--enable-features=EnablePerfettoSystemTracing')
  return flags


def _Run(device: device_utils.DeviceUtils, parsed_config, output_dir_name,
         runs_to_perform):
  configs = _FlattenParsedConfig(parsed_config)
  # On the device create the directory for Chrome to watch.
  _PrepareNotification(device)

  csv_file = os.path.join(output_dir_name, 'result.csv')
  logcat_dir = os.path.join(output_dir_name, 'logcats')
  os.makedirs(logcat_dir, exist_ok=True)

  runs_done = 0
  while runs_done != runs_to_perform:
    config = configs[random.randrange(len(configs))]
    logcat_file = os.path.join(logcat_dir,
                               '{:04d}_logcat.txt'.format(runs_done))
    package_info = constants.PACKAGE_INFO['chrome']

    with flag_changer.CustomCommandLineFlags(device, package_info.cmdline_file,
                                             _FlagsForConfig(config)):
      url = config['url']
      if 'named_urls' in config and url in config['named_urls']:
        url = config['named_urls'][url]
      logcat_monitor = device.GetLogcatMonitor(clear=True,
                                               output_file=logcat_file)
      logcat_monitor.Start()
      try:
        device.StartActivity(intent.Intent(package=package_info.package,
                                           activity=package_info.activity,
                                           data=url),
                             blocking=True,
                             force_stop=True)
        # Wait some for the webpage to load.
        time.sleep(7)

        enable_tracing = config.get('enable_chrometto_tracing', False)
        if not enable_tracing:
          _ReplayTouchEvents(device)
        else:
          trace_dir = os.path.join(output_dir_name, 'traces',
                                   '{:04d}_trace'.format(runs_done))
          _ReplayTouchEventsWithCpuProfile(device, trace_dir, config)

        # Save the measurements.
        jank_csv = _ExtractJankCsv(logcat_monitor)
        if jank_csv:
          with open(csv_file, 'a') as writer:
            writer.write(f'{runs_done},{url},{enable_tracing},{jank_csv}\n')

      finally:
        logcat_monitor.Stop()
        logcat_monitor.Close()
        logging.info('Logcat saved at: {}'.format(logcat_file))
    runs_done += 1


def _EnsureOutputDirectoryIsSet(d: str):
  if os.path.exists(d):
    if not os.path.isdir(d):
      raise Exception('Exists and not a directory: {}'.format(d))
    elif len(os.listdir(d)) != 0:
      raise Exception('Output directory exists and not empty: {}'.format(d))
  os.makedirs(d, exist_ok=True)


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateParser()
  args = parser.parse_args()

  _EnsureOutputDirectoryIsSet(args.output)

  if not os.path.isfile(args.config):
    raise Exception('Could not find config file')

  with open(args.config) as f:
    parsed_config = pyyaml.safe_load(f)
  print(json.dumps(parsed_config, indent=4))

  devil_chromium.Initialize()
  device = _GetPreferredDevice(args.device)
  if not device:
    raise Exception('Device not found or not healthy')
  logging.info('Using device: {}'.format(device.serial))

  device.PushChangedFiles(
      [(os.path.abspath(args.events), _EVENTS_FILE_ON_DEVICE),
       (os.path.abspath(args.replayer), _REPLAY_EXECUTABLE_ON_DEVICE)],
      delete_device_stale=True)
  if not device.FileExists(_REPLAY_EXECUTABLE_ON_DEVICE):
    raise Exception('Executable not found on device: {}'.format(
        _REPLAY_EXECUTABLE_ON_DEVICE))

  if args.n == 0:
    raise Exception('Requested 0 runs')
  _Run(device, parsed_config, args.output, args.n)


if __name__ == '__main__':
  main()
