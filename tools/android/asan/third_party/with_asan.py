#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import contextlib
import logging
import os
import subprocess
import sys

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil import base_error
from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from devil.android.sdk import version_codes
from devil.utils import logging_common

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android'))
import devil_chromium

_SCRIPT_PATH = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        'asan_device_setup.sh'))


@contextlib.contextmanager
def _LogDevicesOnFailure(msg):
  try:
    yield
  except base_error.BaseError:
    logging.exception(msg)
    logging.error('Devices visible to adb:')
    for entry in adb_wrapper.AdbWrapper.Devices(desired_state=None,
                                                long_list=True):
      logging.error('  %s: %s',
                    entry[0].GetDeviceSerial(),
                    ' '.join(entry[1:]))
    raise


@contextlib.contextmanager
def Asan(args):
  env = os.environ.copy()
  env['ADB'] = args.adb

  try:
    with _LogDevicesOnFailure('Failed to set up the device.'):
      device = device_utils.DeviceUtils.HealthyDevices(
          device_arg=args.device)[0]
      disable_verity = device.build_version_sdk >= version_codes.MARSHMALLOW
      if disable_verity:
        device.EnableRoot()
        # TODO(crbug.com/790202): Stop logging output after diagnosing
        # issues on android-asan.
        verity_output = device.adb.DisableVerity()
        if verity_output:
          logging.info('disable-verity output:')
          for line in verity_output.splitlines():
            logging.info('  %s', line)
        device.Reboot()
      # Call EnableRoot prior to asan_device_setup.sh to ensure it doesn't
      # get tripped up by the root timeout.
      device.EnableRoot()
      setup_cmd = [_SCRIPT_PATH, '--lib', args.lib]
      if args.device:
        setup_cmd += ['--device', args.device]
      subprocess.check_call(setup_cmd, env=env)
      yield
  finally:
    with _LogDevicesOnFailure('Failed to tear down the device.'):
      device.EnableRoot()
      teardown_cmd = [_SCRIPT_PATH, '--revert']
      if args.device:
        teardown_cmd += ['--device', args.device]
      subprocess.check_call(teardown_cmd, env=env)
      if disable_verity:
        # TODO(crbug.com/790202): Stop logging output after diagnosing
        # issues on android-asan.
        verity_output = device.adb.EnableVerity()
        if verity_output:
          logging.info('enable-verity output:')
          for line in verity_output.splitlines():
            logging.info('  %s', line)
        device.Reboot()


def main(raw_args):
  parser = argparse.ArgumentParser()
  logging_common.AddLoggingArguments(parser)
  parser.add_argument(
      '--adb', type=os.path.realpath, required=True,
      help='Path to adb binary.')
  parser.add_argument(
      '--device',
      help='Device serial.')
  parser.add_argument(
      '--lib', type=os.path.realpath, required=True,
      help='Path to asan library.')
  parser.add_argument(
      'command', nargs='*',
      help='Command to run with ASAN installed.')
  args = parser.parse_args()

  # TODO(crbug.com/790202): Remove this after diagnosing issues
  # with android-asan.
  if not args.quiet:
    args.verbose += 1

  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb)

  with Asan(args):
    if args.command:
      return subprocess.call(args.command)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
