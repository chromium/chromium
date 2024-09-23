#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import logging
import os
import subprocess
import sys

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil import base_error
from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from devil.utils import logging_common

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android'))
import devil_chromium


@contextlib.contextmanager
def _LogDevicesOnFailure(msg):
  try:
    yield
  except base_error.BaseError:
    logging.exception(msg)
    logging.error('Devices visible to adb:')
    for entry in adb_wrapper.AdbWrapper.Devices(desired_state=None,
                                                long_list=True):
      logging.error('  %s: %s', entry[0].GetDeviceSerial(), ' '.join(entry[1:]))
    raise


@contextlib.contextmanager
def Mte(args):
  env = os.environ.copy()
  env['ADB'] = args.adb

  try:
    with _LogDevicesOnFailure('Failed to set up the device.'):
      device = device_utils.DeviceUtils.HealthyDevices(
          device_arg=args.device)[0]
      bootctl_supported = device.GetProp('ro.arm64.memtag.bootctl_supported')
      if bootctl_supported != '1':
        raise Exception('MTE is not supported on this device')
      output = device.SetProp('arm64.memtag.bootctl', 'memtag')
      logging.debug('Recieved the following output from adb: %s' % output)
      device.Reboot()
      yield
  finally:
    with _LogDevicesOnFailure('Failed to tear down the device.'):
      output = device.SetProp('arm64.memtag.bootctl', 'none')
      logging.debug('Recieved the following output from adb: %s' % output)
      device.Reboot()


def main(raw_args):
  parser = argparse.ArgumentParser()
  logging_common.AddLoggingArguments(parser)
  parser.add_argument('--adb',
                      type=os.path.realpath,
                      required=True,
                      help='Path to adb binary.')
  parser.add_argument('--device', help='Device serial.')
  parser.add_argument('command',
                      nargs='*',
                      help='Command to run with MTE enabled.')
  args = parser.parse_args()

  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb)

  with Mte(args):
    if args.command:
      return subprocess.call(args.command)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
