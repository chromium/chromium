#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Triggers and collects data from a dump created by dump_process.cc.

The resulting files are ready to be analyzed by analyze_dumps.py.
"""

import argparse
import logging
import os
import sys

_SRC_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--package', type=str, required=True,
                      help='Chrome package')
  parser.add_argument('--output-directory', type=str, required=True,
                      help='Dumps destination directory')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  logging.info('Configuring the device')
  devices = device_utils.DeviceUtils.HealthyDevices()
  assert len(devices) == 1, 'Expected exactly one connected device'
  device = devices[0]
  device.EnableRoot()
  device.RunShellCommand(('setenforce', '0'))

  logging.info('Finding the first renderer PID')
  renderer_name = '%s:sandboxed_process' % args.package
  renderer_pids = device.GetPids(renderer_name)
  pid = int(renderer_pids.items()[0][1][0])
  logging.info('PID = %d', pid)

  logging.info('Setting up directories')
  dumps_path = '/data/local/tmp/dumps'
  device.RemovePath(dumps_path, force=True, recursive=True)
  device.RunShellCommand(('mkdir', dumps_path))

  logging.info('Dumping the renderer\'s memory')
  command = (
      'mkdir -p %(path)s && cd %(path)s && /data/local/tmp/dump_process %(pid)d'
      % {'path': dumps_path, 'pid': pid})
  device.RunShellCommand(command, shell=True)

  logging.info('Pulling results')
  filenames = device.ListDirectory(dumps_path)
  for filename in filenames:
    device_path = os.path.join(dumps_path, filename)
    device.PullFile(device_path, os.path.join(args.output_directory, filename))


if __name__ == '__main__':
  sys.exit(main())
