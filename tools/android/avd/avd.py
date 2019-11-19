#! /usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(
    os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil.android.tools import script_common
from devil.utils import logging_common

sys.path.append(
    os.path.join(_SRC_ROOT, 'build', 'android'))
import devil_chromium
from pylib.local.emulator import avd


def main(raw_args):

  parser = argparse.ArgumentParser()

  def add_common_arguments(parser):
    logging_common.AddLoggingArguments(parser)
    script_common.AddEnvironmentArguments(parser)
    parser.add_argument(
        '--avd-config',
        type=os.path.realpath,
        metavar='PATH',
        required=True,
        help='Path to an AVD config text protobuf.')

  subparsers = parser.add_subparsers()
  install_parser = subparsers.add_parser(
      'install',
      help='Install the CIPD packages specified in the given config.')
  add_common_arguments(install_parser)

  def install_cmd(args):
    avd.AvdConfig(args.avd_config).Install()
    return 0

  install_parser.set_defaults(func=install_cmd)

  create_parser = subparsers.add_parser(
      'create',
      help='Create an AVD CIPD package according to the given config.')
  add_common_arguments(create_parser)
  create_parser.add_argument(
      '--snapshot',
      action='store_true',
      help='Snapshot the AVD before creating the CIPD package.')
  create_parser.add_argument(
      '--force',
      action='store_true',
      help='Pass --force to AVD creation.')
  create_parser.add_argument(
      '--keep',
      action='store_true',
      help='Keep the AVD after creating the CIPD package.')
  create_parser.add_argument(
      '--cipd-json-output',
      type=os.path.realpath,
      metavar='PATH',
      help='Path to which `cipd create` should dump json output '
           'via -json-output.')

  def create_cmd(args):
    avd.AvdConfig(args.avd_config).Create(
        force=args.force, snapshot=args.snapshot, keep=args.keep,
        cipd_json_output=args.cipd_json_output)
    return 0

  create_parser.set_defaults(func=create_cmd)

  start_parser = subparsers.add_parser(
      'start',
      help='Start an AVD instance with the given config.')
  add_common_arguments(start_parser)

  def start_cmd(args):
    inst = avd.AvdConfig(args.avd_config).CreateInstance()
    inst.Start(read_only=False, snapshot_save=True)
    print('%s started (pid: %d)' % (str(inst), inst._emulator_proc.pid))
    return 0

  start_parser.set_defaults(func=start_cmd)

  args = parser.parse_args(raw_args)
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
