# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script runs Android perfetto tracing, located in the
third_party/catapult folder. It is consumed as a library by the
executable profile_chrome_startup script.
"""

import logging
import os
import subprocess
import sys
import webbrowser

_CATAPULT_DIR = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             'third_party', 'catapult')

sys.path.insert(0, os.path.join(_CATAPULT_DIR, 'systrace'))
sys.path.insert(0, os.path.join(_CATAPULT_DIR, 'devil'))

from profile_chrome import chrome_startup_tracing_agent
from profile_chrome import profiler
from profile_chrome import flags
from systrace import util
from devil.android import device_utils
from devil.android.sdk import adb_wrapper

_CHROME_STARTUP_MODULES = [chrome_startup_tracing_agent]


def ProfileChrome(options):
  """Profiles chrome on Android and saves the trace file to output file.

  Args:
    options: Command line flags with their specified values as
        returned by optparse.

  Returns:
    Path to Android profetto trace file
  """
  if not options.device_serial_number:
    # Find the serial number of the connected device.
    devices = [a.GetDeviceSerial() for a in adb_wrapper.AdbWrapper.Devices()]
    if len(devices) == 0:
      raise RuntimeError('No ADB devices connected.')
    elif len(devices) >= 2:
      raise RuntimeError('Multiple devices connected, serial number ' +
                         'required. Specify the -e flag.')
    options.device_serial_number = devices[0]

  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)

  # Check if the device is healthy.
  devices = device_utils.DeviceUtils.HealthyDevices()
  device = None
  for d in devices:
    if d.serial == options.device_serial_number:
      device = d
      break
  if device is None:
    raise RuntimeError('No valid connected devices. Check the device ' +
                       'serial number.')
  package_info = util.get_supported_browsers()[options.browser]

  options.device = device
  options.package_info = package_info

  # Ensure compatibility between trace_format and write_json flags.
  # trace_format is preferred. write_json is supported for backward
  # compatibility reasons.
  flags.ParseFormatFlags(options)

  trace_file = profiler.CaptureProfile(options,
                                       options.trace_time,
                                       _CHROME_STARTUP_MODULES,
                                       output=options.output_file,
                                       compress=options.compress,
                                       trace_format=options.trace_format)

  if options.view:
    _DisplayInBrowser(options, trace_file)

  return trace_file


def _DisplayInBrowser(options, trace_file):
  """Displays trace in browser.

  Args:
    options: Command line flags with their specified values as
        returned by optparse.
    trace_file: Saved trace filename.
  """
  if options.trace_format == 'proto':
    open_trace_ui_path = os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir,
        'third_party/perfetto/tools/open_trace_in_ui')
    trace_file_path = os.path.join(os.path.dirname(__file__), os.pardir,
                                   os.pardir, trace_file)
    cmd = [open_trace_ui_path, '-i', trace_file_path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    if p.returncode != 0:
      raise RuntimeError('failed: ' + stderr)
  elif sys.platform == 'darwin':
    os.system('/usr/bin/open %s' % os.path.abspath(trace_file))
  else:
    webbrowser.open(trace_file)
