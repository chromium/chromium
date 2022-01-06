#! /usr/bin/env vpython3
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
      '--privileged-apk',
      action='append',
      default=[],
      dest='privileged_apk_pairs',
      nargs=2,
      metavar=('APK_PATH', 'DEVICE_PARTITION'),
      help='Privileged apks to be installed during AVD launching. Expecting '
      'two strings where the first element being the path to the APK, and the '
      'second element being the system image partition on device where the APK '
      'will be pushed to. Example: --privileged-apk path/to/some.apk /system')
  create_parser.add_argument(
      '--additional-apk',
      action='append',
      default=[],
      dest='additional_apks',
      metavar='APK_PATH',
      type=os.path.realpath,
      help='Additional apk to be installed during AVD launching')
  create_parser.add_argument(
      '--cipd-json-output',
      type=os.path.realpath,
      metavar='PATH',
      help='Path to which `cipd create` should dump json output '
           'via -json-output.')
  create_parser.add_argument(
      '--dry-run',
      action='store_true',
      help='Skip the CIPD package creation after creating the AVD.')

  def create_cmd(args):
    avd.AvdConfig(args.avd_config).Create(
        force=args.force,
        snapshot=args.snapshot,
        keep=args.keep,
        additional_apks=args.additional_apks,
        privileged_apk_tuples=[tuple(p) for p in args.privileged_apk_pairs],
        cipd_json_output=args.cipd_json_output,
        dry_run=args.dry_run)
    return 0

  create_parser.set_defaults(func=create_cmd)

  start_parser = subparsers.add_parser(
      'start',
      help='Start an AVD instance with the given config.',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  start_parser.add_argument(
      '--wipe-data',
      action='store_true',
      default=False,
      help='Reset user data image for this emulator. Note that when set, all '
      'the customization, e.g. wifi, additional apks, privileged apks will be '
      'gone')
  start_parser.add_argument(
      '--read-only',
      action='store_true',
      help='Allowing running multiple instances of emulators on the same AVD, '
      'but cannot save snapshot. This will be forced to False if emulator '
      'has a system snapshot.')
  start_parser.add_argument(
      '--no-read-only',
      action='store_false',
      dest='read_only')
  # TODO(crbug.com/1278096): Default to False when AVDs with sideloaded
  # system apks are rolled.
  start_parser.set_defaults(read_only=True)
  start_parser.add_argument(
      '--writable-system',
      action='store_true',
      default=False,
      help='Makes system & vendor image writable after adb remount. This will '
      'be forced to True, if emulator has a system snapshot.')
  start_parser.add_argument(
      '--emulator-window',
      action='store_true',
      default=False,
      help='Enable graphical window display on the emulator.')
  start_parser.add_argument(
      '--gpu-mode',
      default='swiftshader_indirect',
      help='Override the mode of hardware OpenGL ES emulation indicated by the '
      'AVD. See "emulator -help-gpu" for a full list of modes. Note when set '
      'to "host", it needs a valid DISPLAY env, even if "--emulator-window" is '
      'false, and it will not work under remote sessions like chrome remote '
      'desktop.')
  start_parser.add_argument(
      '--debug-tags',
      help='Comma-separated list of debug tags. This can be used to enable or '
      'disable debug messages from specific parts of the emulator, e.g. '
      'init, snapshot. See "emulator -help-debug-tags" '
      'for a full list of tags.')
  add_common_arguments(start_parser)

  def start_cmd(args):
    inst = avd.AvdConfig(args.avd_config).CreateInstance()
    inst.Start(read_only=args.read_only,
               window=args.emulator_window,
               writable_system=args.writable_system,
               gpu_mode=args.gpu_mode,
               wipe_data=args.wipe_data,
               debug_tags=args.debug_tags)
    print('%s started (pid: %d)' % (str(inst), inst._emulator_proc.pid))
    return 0

  start_parser.set_defaults(func=start_cmd)

  args = parser.parse_args(raw_args)
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
