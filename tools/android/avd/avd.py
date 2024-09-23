#! /usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import logging
import pathlib
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


def _add_avd_config_argument(parser):
  parser.add_argument('--avd-config',
                      type=os.path.realpath,
                      metavar='PATH',
                      required=True,
                      help='Path to an AVD config text protobuf.')


def _add_common_arguments(parser):
  logging_common.AddLoggingArguments(parser)
  script_common.AddEnvironmentArguments(parser)


def main(raw_args):

  parser = argparse.ArgumentParser()

  subparsers = parser.add_subparsers()
  subparser = subparsers.add_parser(
      'install',
      help='Install the CIPD packages specified in the given config.')
  _add_common_arguments(subparser)
  _add_avd_config_argument(subparser)

  def install_cmd(args):
    avd.AvdConfig(args.avd_config).Install()
    return 0

  subparser.set_defaults(func=install_cmd)

  subparser = subparsers.add_parser(
      'uninstall',
      help='Uninstall all the artifacts associated with the given config.')
  _add_common_arguments(subparser)
  _add_avd_config_argument(subparser)

  def uninstall_cmd(args):
    avd.AvdConfig(args.avd_config).Uninstall()
    return 0

  subparser.set_defaults(func=uninstall_cmd)

  subparser = subparsers.add_parser(
      'create',
      help='Create an AVD CIPD package according to the given config.')
  _add_common_arguments(subparser)
  _add_avd_config_argument(subparser)
  subparser.add_argument(
      '--snapshot',
      action='store_true',
      help='Snapshot the AVD before creating the CIPD package.')
  subparser.add_argument('--force',
                         action='store_true',
                         help='Pass --force to AVD creation.')
  subparser.add_argument('--keep',
                         action='store_true',
                         help='Keep the AVD after creating the CIPD package.')
  subparser.add_argument(
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
  subparser.add_argument(
      '--additional-apk',
      action='append',
      default=[],
      dest='additional_apks',
      metavar='APK_PATH',
      type=os.path.realpath,
      help='Additional apk to be installed during AVD launching')
  subparser.add_argument(
      '--cipd-json-output',
      type=os.path.realpath,
      metavar='PATH',
      help='Path to which `cipd create` should dump json output '
      'via -json-output.')
  subparser.add_argument(
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

  subparser.set_defaults(func=create_cmd)

  subparser = subparsers.add_parser(
      'start',
      help='Start an AVD instance with the given config.',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  _add_common_arguments(subparser)
  _add_avd_config_argument(subparser)
  subparser.add_argument(
      '--wipe-data',
      action='store_true',
      default=False,
      help='Reset user data image for this emulator. Note that when set, all '
      'the customization, e.g. wifi, additional apks, privileged apks will be '
      'gone')
  subparser.add_argument(
      '--read-only',
      action='store_true',
      help='Allowing running multiple instances of emulators on the same AVD, '
      'but cannot save snapshot. This will be forced to False if emulator '
      'has a system snapshot.')
  subparser.add_argument('--no-read-only',
                         action='store_false',
                         dest='read_only')
  # TODO(crbug.com/40208043): Default to False when AVDs with sideloaded
  # system apks are rolled.
  subparser.set_defaults(read_only=True)
  subparser.add_argument(
      '--writable-system',
      action='store_true',
      default=False,
      help='Makes system & vendor image writable after adb remount. This will '
      'be forced to True, if emulator has a system snapshot.')
  subparser.add_argument(
      '--emulator-window',
      action='store_true',
      default=False,
      help='Enable graphical window display on the emulator.')
  subparser.add_argument(
      '--gpu-mode',
      help='Override the mode of hardware OpenGL ES emulation indicated by the '
      'AVD. See "emulator -help-gpu" for a full list of modes. Note when set '
      'to "host", it needs a valid DISPLAY env, even if "--emulator-window" is '
      'false, and it will not work under remote sessions like chrome remote '
      'desktop.')
  subparser.add_argument(
      '--debug-tags',
      help='Comma-separated list of debug tags. This can be used to enable or '
      'disable debug messages from specific parts of the emulator, e.g. '
      'init,snapshot. See "emulator -help-debug-tags" '
      'for a full list of tags.')
  subparser.add_argument(
      '--disk-size',
      help='Override the default disk size for the emulator instance.')
  subparser.add_argument(
      '--enable-network',
      action='store_true',
      help='Enable the network (WiFi and mobile data) on the emulator.')
  subparser.add_argument(
      '--require-fast-start',
      action='store_true',
      help='Shortens the start-up timeout and turns off the customization for '
      'local AVD run, e.g. larger disk space. Should be set when used by bots '
      'for AVD create or start.')

  def start_cmd(args):
    avd_config = avd.AvdConfig(args.avd_config)
    if not avd_config.IsAvailable():
      logging.warning('Emulator not up-to-date, installing (takes a minute)...')
      avd_config.Install()
      logging.warning('Starting emulator...')

    debug_tags = args.debug_tags
    if not debug_tags and args.verbose:
      debug_tags = 'time,init'

    inst = avd_config.CreateInstance()
    inst.Start(read_only=args.read_only,
               window=args.emulator_window,
               writable_system=args.writable_system,
               gpu_mode=args.gpu_mode,
               wipe_data=args.wipe_data,
               debug_tags=debug_tags,
               disk_size=args.disk_size,
               enable_network=args.enable_network,
               require_fast_start=args.require_fast_start)
    print('%s started (pid: %d)' % (str(inst), inst._emulator_proc.pid))
    return 0

  subparser.set_defaults(func=start_cmd)

  subparser = subparsers.add_parser(
      'list',
      help='Shows possible values for --avd-config.',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  _add_common_arguments(subparser)

  def list_cmd(args):
    files = pathlib.Path(__file__).parent.glob('proto/*.textpb')
    avd_configs = [avd.AvdConfig(os.path.relpath(f)) for f in files]
    avd_configs.sort(key=lambda c: c.avd_proto_path)
    fmt_string = '{:70} {:40} {}'
    print(
        fmt_string.format('Possible values for --avd-config:', 'Name',
                          'Active & Up-to-date'))
    for avd_config in avd_configs:
      print(
          fmt_string.format(avd_config.avd_proto_path, avd_config.avd_name,
                            avd_config.IsAvailable()))
    print()
    print('Warning: playstore images currently require --wipe-data. '
          'See: https://crbug.com/1116196')
    return 0

  subparser.set_defaults(func=list_cmd)

  if len(sys.argv) == 1:
    parser.print_help()
    return 1

  args = parser.parse_args(raw_args)

  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
