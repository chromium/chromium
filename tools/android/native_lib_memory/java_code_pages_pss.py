#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the total PSS attributed to another app in Chrome's mappings.

This script assumes a device with Monochrome, and requires root access.
For instance, to get the part of Chrome's memory footprint coming from GMSCore
code and bytecode pages:
$ tools/android/native_lib_memory/java_code_pages_pss.py
    --chrome-package com.android.chrome
    --app-package com.google.android.gms --verbose
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


def _GetPssInKb(mappings, app_package, verbose):
  """Returns the total PSS from mappings.

  Args:
    mappings: ([parse_smaps.Mapping]) List of mappings.
    app_package: (str) App package to look for.
    verbose: (bool) Verbose output or not.

  Returns:
    (executable_pss (int), other_pss (int)) Executable mappings and others,
                                            in kB.
  """
  executable_pss, other_pss = (0, 0)
  for mapping in mappings:
    if app_package in mapping.pathname:
      if mapping.permissions == 'r-xp':
        executable_pss += mapping.fields['Pss']
      else:
        other_pss += mapping.fields['Pss']
      if verbose:
        print(mapping.ToString())
  return (executable_pss, other_pss)


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--chrome-package', help='Chrome package to look for.',
                      required=True)
  parser.add_argument('--app-package', help='Application to inspect.',
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
  processes = device.ListProcesses(args.chrome_package)
  logging.basicConfig(level=logging.INFO)
  logging.info('Processes:\n\t' + '\n\t'.join(p.name for p in processes))
  total_executable_pss_kb, total_other_pss_kb = (0, 0)
  for process in processes:
    mappings = parse_smaps.ParseProcSmaps(device, process.pid)
    executable_pss_kb, other_pss_kb = _GetPssInKb(
        mappings, args.app_package, args.verbose)
    total_executable_pss_kb += executable_pss_kb
    total_other_pss_kb += other_pss_kb

  print('Total executable PSS = %dkB' % total_executable_pss_kb)
  print('Total other mappings PSS = %dkB' % total_other_pss_kb)


if __name__ == '__main__':
  main()
