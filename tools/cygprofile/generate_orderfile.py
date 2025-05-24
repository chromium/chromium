#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs benchmarks and generates an orderfile, similar to generate_profile.py.

Example:
Build trichrome_chrome_64_32_bundle and install it on device.

Run this script with:
$ tools/cygprofile/generate_orderfile.py -C out/orderfile-arm64 \
    --android-browser android-trichrome-chrome-64-32-bundle \
    --target-arch arm64

The orderfiles should be located in out/orderfile-arm64/orderfiles.
"""

import argparse
import logging
import pathlib
import sys

import process_profiles
import android_profile_tool
import cluster

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils


def _ReadNonEmptyStrippedFromFile(file_name):
  stripped_lines = []
  with open(file_name, 'r') as file:
    for line in file:
      stripped_line = line.strip()
      if stripped_line:
        stripped_lines.append(stripped_line)
  return stripped_lines


def _AddDummyFunctions(options):
  symbols = _ReadNonEmptyStrippedFromFile(
      _GetUnpatchedOrderfileFilename(options))
  with open(_GetOrderfileFilename(options), 'w') as f:
    # Make sure the anchor functions are located in the right place, here and
    # after everything else.
    # See the comment in //base/android/library_loader/anchor_functions.cc.
    f.write('dummy_function_start_of_ordered_text\n')
    for sym in symbols:
      f.write(sym + '\n')
    f.write('dummy_function_end_of_ordered_text\n')


def _GetOrderfilesDir(options) -> pathlib.Path:
  if options.isolated_script_test_output:
    orderfiles_dir = options.isolated_script_test_output.parent / 'orderfiles'
  else:
    orderfiles_dir = options.out_dir / 'orderfiles'
  orderfiles_dir.mkdir(exist_ok=True)
  return orderfiles_dir


def _GetOrderfileFilename(options):
  """Gets the path to the architecture-specific orderfile."""
  arch = options.arch
  orderfiles_dir = _GetOrderfilesDir(options)
  return str(orderfiles_dir / f'orderfile.{arch}.out')


def _GetUnpatchedOrderfileFilename(options):
  """Gets the path to the architecture-specific unpatched orderfile."""
  arch = options.arch
  orderfiles_dir = _GetOrderfilesDir(options)
  return str(orderfiles_dir / f'unpatched_orderfile.{arch}')


def GenerateAndProcessProfile(options):
  """Invokes a script to merge the per-thread traces into one file.

  The produced list of offsets is saved in the orderfile.
  """
  if options.verbosity >= 2:
    level = logging.DEBUG
  elif options.verbosity == 1:
    level = logging.INFO
  else:
    level = logging.WARNING
  logging.basicConfig(level=level,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  logging.info('Generate Profile Data')

  # Ensure that the output directory is an absolute path.
  options.out_dir = options.out_dir.resolve(strict=True)
  logging.info('Using options.out_dir=%s', options.out_dir)

  devices = device_utils.DeviceUtils.HealthyDevices()
  assert devices, 'Expected at least one connected device'
  device = devices[0]

  host_profile_root = options.out_dir / 'profile_data'
  profiler = android_profile_tool.AndroidProfileTool(
      str(host_profile_root),
      device,
      debug=options.streamline_for_debugging,
      verbosity=options.verbosity)

  files = []
  logging.getLogger().setLevel(logging.DEBUG)

  # Chrome targets
  libchrome_target = 'libmonochrome'
  if '64' in options.arch:
    # Monochrome has a _64 suffix for arm64 and x64 builds.
    libchrome_target += '_64'
  lib_chrome_so = str(options.out_dir / f'lib.unstripped/{libchrome_target}.so')

  if options.arch == 'arm64':
    files = profiler.CollectSpeedometerProfile(options.android_browser,
                                               str(options.out_dir))
  else:
    files = profiler.CollectSystemHealthProfile(options.android_browser,
                                                str(options.out_dir))

  try:
    profiles = process_profiles.ProfileManager(files)
    processor = process_profiles.SymbolOffsetProcessor(lib_chrome_so)
    ordered_symbols = cluster.ClusterOffsets(profiles, processor)
    if not ordered_symbols:
      raise Exception('Failed to get ordered symbols')
    for sym in ordered_symbols:
      assert not sym.startswith('OUTLINED_FUNCTION_'), (
          'Outlined function found in instrumented function, very likely '
          'something has gone very wrong!')

    with open(_GetUnpatchedOrderfileFilename(options), 'w') as orderfile:
      orderfile.write('\n'.join(ordered_symbols))
  finally:
    if not options.save_profile_data:
      profiler.Cleanup()
    logging.getLogger().setLevel(logging.INFO)

  _AddDummyFunctions(options)


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()

  # Essential arguments for profiling and processing:
  parser.add_argument('--target-arch',
                      dest='arch',
                      required=True,
                      choices=['arm', 'arm64', 'x86', 'x64'],
                      help='The target architecture for which to build.')
  parser.add_argument('--android-browser',
                      required=True,
                      help='Browser string to pass to run_benchmark.')
  parser.add_argument('-C',
                      '--out-dir',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the output directory (e.g. out/Release).')
  parser.add_argument('--save-profile-data',
                      action='store_true',
                      default=False,
                      help='Avoid deleting out/Release/profile_data.')
  parser.add_argument('--streamline-for-debugging',
                      action='store_true',
                      help=('Streamline the run for faster debugging.'))
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbosity',
                      action='count',
                      default=0,
                      help='Increase verbosity for debugging.')
  # The following two are bot-specific args.
  parser.add_argument('--isolated-script-test-output',
                      type=pathlib.Path,
                      help='Output.json file that the script can write to.')
  parser.add_argument('--isolated-script-test-perf-output',
                      help='Deprecated and ignored, but bots pass it.')

  return parser


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
  GenerateAndProcessProfile(options)
  return 0


if __name__ == '__main__':
  sys.exit(main())
