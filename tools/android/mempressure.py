#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import logging
import optparse
import os
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_errors
from devil.android import device_utils
from devil.android import flag_changer
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium
from pylib import constants

# Browser Constants
DEFAULT_BROWSER = 'chrome'

# Action Constants
ACTION_PACKAGE = 'org.chromium.base'
ACTION_TRIM = {
    'moderate' : ACTION_PACKAGE + '.ACTION_TRIM_MEMORY_MODERATE',
    'critical' : ACTION_PACKAGE + '.ACTION_TRIM_MEMORY_RUNNING_CRITICAL',
    'complete' : ACTION_PACKAGE + '.ACTION_TRIM_MEMORY'
}
ACTION_LOW = ACTION_PACKAGE + '.ACTION_LOW_MEMORY'

# Command Line Constants
ENABLE_TEST_INTENTS_FLAG = '--enable-test-intents'

def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('-l',
                           '--low',
                           help='Simulate Activity#onLowMemory()',
                           action='store_true')
  option_parser.add_option('-t',
                           '--trim',
                           help=('Simulate Activity#onTrimMemory(...) with ' +
                                 ', '.join(ACTION_TRIM.keys())),
                           type='string')
  option_parser.add_option('-b',
                           '--browser',
                           default=DEFAULT_BROWSER,
                           help=('Which browser to use. One of ' +
                                 ', '.join(constants.PACKAGE_INFO.keys()) +
                                 ' [default: %default]'),
                           type='string')

  (options, args) = option_parser.parse_args(argv)

  if len(args) > 1:
    print('Unknown argument: ', args[1:])
    option_parser.print_help()
    sys.exit(1)

  if options.low and options.trim:
    option_parser.error('options --low and --trim are mutually exclusive')

  if not options.low and not options.trim:
    option_parser.print_help()
    sys.exit(1)

  action = None
  if options.low:
    action = ACTION_LOW
  elif options.trim in ACTION_TRIM.keys():
    action = ACTION_TRIM[options.trim]

  if action is None:
    option_parser.print_help()
    sys.exit(1)

  if not options.browser in constants.PACKAGE_INFO.keys():
    option_parser.error('Unknown browser option ' + options.browser)

  devil_chromium.Initialize()

  package_info = constants.PACKAGE_INFO[options.browser]

  package = package_info.package
  activity = package_info.activity

  devices = device_utils.DeviceUtils.HealthyDevices()
  if not devices:
    raise device_errors.NoDevicesError()
  elif len(devices) > 1:
    logging.warning('Multiple devices attached. Using %s.', str(devices[0]))
  device = devices[0]

  try:
    device.EnableRoot()
  except device_errors.CommandFailedError as e:
    # Try to change the flags and start the activity anyway.
    # TODO(jbudorick) Handle this exception appropriately after interface
    #                 conversions are finished.
    logging.error(str(e))
  flags = flag_changer.FlagChanger(device, package_info.cmdline_file)
  flags.AddFlags([ENABLE_TEST_INTENTS_FLAG])

  device.StartActivity(intent.Intent(package=package, activity=activity,
                                     action=action))

if __name__ == '__main__':
  sys.exit(main(sys.argv))
