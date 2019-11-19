#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the total PSS attributed to Chrome's code pages in an application.

This script assumes a device with Monochrome, and requires root access.
For instance, to get chrome's code page memory footprint:
$ tools/android/native_lib_memory/code_pages_pss.py
    --app-package com.android.chrome
    --chrome-package com.android.chrome --verbose

To get Webview's footprint in AGSA:
$ tools/android/native_lib_memory/code_pages_pss.py
    --app-package com.google.android.googlequicksearchbox
    --chrome-package com.android.chrome --verbose
"""

from __future__ import print_function

import argparse
import logging
import os
import re
import sys

import parse_smaps

_SRC_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils


def _GetPssInKb(mappings, chrome_package, verbose):
  pss = 0
  for mapping in mappings:
    if chrome_package in mapping.pathname and mapping.permissions == 'r-xp':
      pss += mapping.fields['Pss']
      if verbose:
        print(mapping.ToString())
  return pss


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--app-package', help='Application to inspect.',
                      required=True)
  parser.add_argument('--chrome-package', help='Chrome package to look for.',
                      required=True)
  parser.add_argument('--verbose', help='Verbose output.',
                      action='store_true')
  return parser


def main():
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  devices = device_utils.DeviceUtils.HealthyDevices()
  if not devices:
    logging.error('No connected devices')
    return
  device = devices[0]
  device.EnableRoot()
  processes = device.ListProcesses(args.app_package)
  logging.basicConfig(level=logging.INFO)
  logging.info('Processes:\n\t' + '\n\t'.join(p.name for p in processes))
  total_pss_kb = 0
  for process in processes:
    mappings = parse_smaps.ParseProcSmaps(device, process.pid)
    total_pss_kb += _GetPssInKb(mappings, args.chrome_package, args.verbose)
  print('Total PSS from code pages = %dkB' % total_pss_kb)


if __name__ == '__main__':
  main()
