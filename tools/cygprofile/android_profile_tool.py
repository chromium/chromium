#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility library for collecting orderfile profile on an Android device.

Allows to disable sandboxing (in Chrome and on the device), run a few hardcoded
workloads, pull orderfile profile files from the device.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
from typing import List

_SRC_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import apk_helper
from devil.android import device_errors
from devil.android import device_utils
from devil.android import flag_changer
from devil.android import forwarder

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium
from pylib import constants


class NoProfileDataError(Exception):
  """An error used to indicate that no profile data was collected."""

  def __init__(self, value):
    super().__init__()
    self.value = value

  def __str__(self):
    return repr(self.value)


class AndroidProfileTool:
  """A utility for generating orderfile profile data for Chrome on Android.

  Does profiling runs, and pulls the data to the local machine.
  """

  _DEVICE_PROFILE_DIR = '/data/local/tmp/chrome/orderfile'

  def __init__(self,
               host_profile_root: str,
               device: device_utils.DeviceUtils,
               debug=False,
               verbosity=0):
    """Constructor.

    Args:
      host_profile_root: Where to store the profiles on the host.
      device: Android device selected to be used to
                            generate orderfile.
      debug: Use simpler, non-representative debugging profile.
      verbosity: The number of -v to pass to telemetry calls.
    """
    assert device, 'Expected a valid device'
    self._device = device
    self._host_profile_root = host_profile_root
    self._debug = debug
    self._verbosity = verbosity
    self._SetUpDevice()
    self._pregenerated_profiles = None

  def SetPregeneratedProfiles(self, files: List[str]):
    """Set pregenerated profiles.

    The pregenerated files will be returned as profile data instead of running
    an actual profiling step.

    Args:
      files: List of pregenerated files.
    """
    logging.info('Using pregenerated profiles')
    self._pregenerated_profiles = files

  def CollectSystemHealthProfile(self, apk: str):
    """Run the orderfile system health benchmarks and collect log files.

    Args:
      apk: The location of the chrome apk file to profile.

    Returns:
      A list of cygprofile data files.

    Raises:
      NoProfileDataError: No data was found on the device.
    """
    if self._pregenerated_profiles:
      logging.info('Using pregenerated profiles instead of running '
                   'system health profile')
      logging.info('Profile files: %s', '\n'.join(self._pregenerated_profiles))
      return self._pregenerated_profiles
    logging.info('Running system health profile')
    profile_benchmark = 'orderfile_generation.training'
    if self._debug:
      logging.info('Using reduced debugging profile')
      profile_benchmark = 'orderfile_generation.debugging'
    self._SetUpDeviceFolders()
    cmd = [
        'tools/perf/run_benchmark', '--device', self._device.serial,
        '--browser=exact', '--browser-executable', apk, profile_benchmark
    ] + ['-v'] * self._verbosity
    logging.debug('Running telemetry command: %s', cmd)
    self._RunCommand(cmd)
    data = self._PullProfileData(profile_benchmark)
    self._DeleteDeviceData()
    return data

  def CollectSpeedometerProfile(self, apk: str):
    """Run Speedometer 3 and collect log files

    Args:
      apk: The location of the chrome apk to profile.

    Returns:
      A list of cygprofile data files
    """
    logging.info('Running Speedometer 3 profile')
    profile_benchmark = 'orderfile_generation.speedometer3'
    if self._debug:
      logging.info('Using reduced debugging profile')
      profile_benchmark = 'orderfile_generation.speedometer3_debugging'
    self._SetUpDeviceFolders()
    cmd = [
        'tools/perf/run_benchmark', '--device', self._device.serial,
        '--browser=exact', '--browser-executable', apk, profile_benchmark
    ] + ['-v'] * self._verbosity
    logging.debug('Running telemetry command: %s', cmd)
    self._RunCommand(cmd)
    data = self._PullProfileData(profile_benchmark)
    self._DeleteDeviceData()
    return data

  def CollectWebViewStartupProfile(self, apk: str):
    """Run the given benchmark and collect the generated profiles.

    Args:
      apk: The location of the webview apk file to profile.

    Returns:
      A list of profile hitmaps.

    Raises:
      NoProfileDataError: No data was found on the device
    """
    # TODO(rasikan): Add support for pregenerated profiles.
    logging.info('Running webview startup profile')
    self._SetUpDeviceFolders()

    package_info = self._GetPackageInfo(apk)
    changer = self._SetCommandLineFlags(package_info)

    chromium_out_dir = os.path.abspath(os.path.join(os.path.dirname(apk), '..'))
    browser = self._GetBrowserFromApk(apk)

    profile_benchmark = 'orderfile_generation.webview_startup'
    if self._debug:
      logging.info('Using reduced debugging profile')
      profile_benchmark = 'orderfile_generation.webview_startup_debugging'
    self._RunCommand([
        'tools/perf/run_benchmark', '--device', self._device.serial,
        '--browser', browser, '--chromium-output-directory', chromium_out_dir,
        profile_benchmark
    ])
    self._RestoreCommandLineFlags(changer)

    data = self._PullProfileData(profile_benchmark)
    self._DeleteDeviceData()
    return data

  def InstallAndSetWebViewProvider(self, installer_path: str):
    """Installs the built WebView on the device and set it as the WebView
    provider.
    public instructions: https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/build-instructions.md#installing-webview-and-switching-provider # pylint: disable=line-too-long
    """
    # Uninstall the existing WebView package to avoid signatures issues.
    self._device.Uninstall('com.google.android.webview.debug')
    self._RunCommand([installer_path, 'install'])
    self._RunCommand([installer_path, 'set-webview-provider'])

  @staticmethod
  def _GetBrowserFromApk(apk: str):
    browser = 'android-webview'
    apk_name = os.path.basename(apk)
    if 'TrichromeWebView' in apk_name:
      browser = browser + '-trichrome'
    if 'Monochrome.apk' in apk_name or 'Google' in apk_name:
      browser = browser + '-google'
    return browser

  @classmethod
  def _RunCommand(cls, command: List[str]):
    """Run a command from current build directory root.

    Args:
      command: A list of command strings.

    Returns:
      The process's return code.
    """
    root = constants.DIR_SOURCE_ROOT
    logging.info('Executing %s in %s', ' '.join(command), root)
    process = subprocess.Popen(command, cwd=root, env=os.environ)
    process.wait()
    return process.returncode

  def Cleanup(self):
    """Delete all local and device files left over from profiling. """
    self._DeleteDeviceData()
    self._DeleteHostData(self._host_profile_root)

  def _SetUpDevice(self):
    """When profiling, files are output to the disk by every process.  This
    means running without sandboxing enabled.
    """
    # We need to have adb root in order to pull profile data
    try:
      logging.info('Enabling root...')
      self._device.EnableRoot()
      # SELinux need to be in permissive mode, otherwise the process cannot
      # write the log files.
      logging.info('Putting SELinux in permissive mode...')
      self._device.RunShellCommand(['setenforce', '0'], check_return=True)
    except device_errors.CommandFailedError as e:
      # TODO(jbudorick) Handle this exception appropriately once interface
      #                 conversions are finished.
      logging.error(str(e))

  @staticmethod
  def _GetPackageInfo(apk_path: str):
    apk = apk_helper.ApkHelper(apk_path)
    for _, p in constants.PACKAGE_INFO.items():
      if p.package == apk.GetPackageName():
        return p
    raise Exception('Unable to determine package info for %s' % apk_path)

  def _SetCommandLineFlags(self, package_info):
    logging.info('Setting command line flags for %s...', package_info.package)
    changer = flag_changer.FlagChanger(self._device, package_info.cmdline_file)
    changer.AddFlags(['--no-sandbox', '--disable-fre'])
    return changer

  def _RestoreCommandLineFlags(self, changer):
    logging.info('Restoring command line flags...')
    if changer:
      changer.Restore()

  def _SetUpDeviceFolders(self):
    """Creates folders on the device to store profile data."""
    logging.info('Setting up device folders...')
    self._DeleteDeviceData()
    self._device.RunShellCommand(['mkdir', '-p', self._DEVICE_PROFILE_DIR],
                                 check_return=True)

  def _DeleteDeviceData(self):
    """Clears out profile storage locations on the device. """
    for profile_dir in [self._DEVICE_PROFILE_DIR]:
      self._device.RunShellCommand(['rm', '-rf', str(profile_dir)],
                                   check_return=True)

  def _DeleteHostData(self, host_profile_dir):
    """Clears out profile storage locations on the host."""
    shutil.rmtree(host_profile_dir, ignore_errors=True)

  def _SetUpHostFolders(self, host_profile_dir):
    self._DeleteHostData(host_profile_dir)
    os.makedirs(host_profile_dir, exist_ok=False)

  def _PullProfileData(self, profile_subdir):
    """Pulls the profile data off of the device.

    Args:
      profile_subdir: The subdirectory name to store the profiles. This is
                      useful when multiple profiles are collected
                      e.g. memory mobile and webview startup will be stored
                      in separate subdirectories in the _host_profile_root.

    Returns:
      A list of profile data files which were pulled.

    Raises:
      NoProfileDataError: No data was found on the device.
    """
    host_profile_dir = self._host_profile_root
    if profile_subdir:
      host_profile_dir = os.path.join(host_profile_dir, profile_subdir)
    logging.info('Pulling profile data into %s...', host_profile_dir)

    self._SetUpHostFolders(host_profile_dir)
    self._device.PullFile(self._DEVICE_PROFILE_DIR,
                          host_profile_dir,
                          timeout=300)

    # After directory pull (over ADB), collect all the profiling-related file
    # names in it. Some old versions of ADB did not create the subdirectory
    # named after the last component of the pulled path (e.g. directory 'd'
    # after 'adb pull /tmp/d') - this case is handled specially. See
    # crbug.com/40484274.
    # TODO(pasko): Stop supporting old versions of ADB and simplify this.
    files = []
    redundant_dir_root = os.path.basename(self._DEVICE_PROFILE_DIR)
    for root_file in os.listdir(host_profile_dir):
      if root_file == redundant_dir_root:
        profile_dir = os.path.join(host_profile_dir, root_file)
        files.extend(
            os.path.join(profile_dir, f) for f in os.listdir(profile_dir))
      else:
        files.append(os.path.join(host_profile_dir, root_file))

    if len(files) == 0:
      raise NoProfileDataError('No profile data was collected')

    return files


def _CreateArgumentParser():
  """Creates and return the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--adb-path', type=os.path.realpath, help='adb binary')
  parser.add_argument('--apk-path',
                      type=os.path.realpath,
                      required=True,
                      help='APK to profile')
  parser.add_argument('--output-directory',
                      type=os.path.realpath,
                      required=True,
                      help='Chromium output directory (e.g. out/Release)')
  parser.add_argument('--trace-directory',
                      type=os.path.realpath,
                      help='Directory in which profile traces will be stored. '
                      'Defaults to <output-directory>/profile_data')
  return parser


def main():
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  devil_chromium.Initialize(output_directory=args.output_directory,
                            adb_path=args.adb_path)

  trace_directory = args.trace_directory
  if not trace_directory:
    trace_directory = os.path.join(args.output_directory, 'profile_data')
  devices = device_utils.DeviceUtils.HealthyDevices()
  assert devices, 'Expected at least one connected device'
  profiler = AndroidProfileTool(host_profile_root=trace_directory,
                                device=devices[0])
  profiler.CollectSystemHealthProfile(args.apk_path)
  return 0


if __name__ == '__main__':
  sys.exit(main())
