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

import android_profile_tool
import orderfile_shared

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils


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


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  orderfile_shared.AddCommonArguments(parser)

  # Essential arguments for profiling and processing:
  parser.add_argument('--android-browser',
                      required=True,
                      help='Browser string to pass to run_benchmark.')
  parser.add_argument('-C',
                      '--out-dir',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the output directory (e.g. out/Release).')
  # The following two are bot-specific args.
  parser.add_argument('--isolated-script-test-output',
                      type=pathlib.Path,
                      help='Output.json file that the script can write to.')
  parser.add_argument('--isolated-script-test-perf-output',
                      help='Deprecated and ignored, but bots pass it.')

  return parser


def GenerateOrderfile(options, device):
  """Generates an orderfile for a given device."""
  host_profile_root = options.out_dir / 'profile_data'
  profiler = android_profile_tool.AndroidProfileTool(
      str(host_profile_root),
      device,
      debug=options.streamline_for_debugging,
      verbosity=options.verbosity)

  lib_chrome_so = orderfile_shared.GetLibchromeSoPath(options.out_dir,
                                                      options.arch,
                                                      options.profile_webview)
  try:
    if options.profile_webview:
      if options.arch == 'arm64':
        webview_target = 'system_webview_64_32_apk'
        apk_name = 'SystemWebView6432.apk'
      else:
        webview_target = 'system_webview_apk'
        apk_name = 'SystemWebView.apk'
      webview_installer_path = str(options.out_dir / 'bin' / webview_target)
      apk_or_browser = str(options.out_dir / 'apks' / apk_name)
    else:
      apk_or_browser = options.android_browser
      webview_installer_path = None
    files = orderfile_shared.CollectProfiles(profiler, options.profile_webview,
                                             options.arch,
                                             apk_or_browser,
                                             str(options.out_dir),
                                             webview_installer_path)
    ordered_symbols, _ = orderfile_shared.ProcessProfiles(files, lib_chrome_so)
    with open(_GetUnpatchedOrderfileFilename(options), 'w') as orderfile:
      orderfile.write('\n'.join(ordered_symbols))
  finally:
    if not options.save_profile_data:
      profiler.Cleanup()
    logging.getLogger().setLevel(logging.INFO)

  orderfile_shared.AddDummyFunctions(_GetUnpatchedOrderfileFilename(options),
                                     _GetOrderfileFilename(options))


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
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

  logging.getLogger().setLevel(logging.DEBUG)
  GenerateOrderfile(options, device)


if __name__ == '__main__':
  main()
