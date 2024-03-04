#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility library for running a startup profile on an Android device.

Sets up a device for cygprofile, disables sandboxing permissions, and sets up
support for web page replay, device forwarding, and fake certificate authority
to make runs repeatable.
"""


import argparse
import logging
import os
import shutil
import subprocess
import sys
import time
from typing import List

_SRC_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import apk_helper
from devil.android import device_errors
from devil.android import device_utils
from devil.android import flag_changer
from devil.android import forwarder
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium
from pylib import constants

sys.path.append(os.path.join(_SRC_PATH, 'tools', 'perf'))
from core import path_util
sys.path.append(path_util.GetTelemetryDir())
from telemetry.internal.util import webpagereplay_go_server
from telemetry.internal.util import binary_manager


class NoProfileDataError(Exception):
  """An error used to indicate that no profile data was collected."""

  def __init__(self, value):
    super().__init__()
    self.value = value

  def __str__(self):
    return repr(self.value)


def _DownloadFromCloudStorage(bucket, sha1_file_name):
  """Download the given file based on a hash file."""
  cmd = ['download_from_google_storage', '--no_resume',
         '--bucket', bucket, '-s', sha1_file_name]
  print('Executing command ' + ' '.join(cmd))
  process = subprocess.Popen(cmd)
  process.wait()
  if process.returncode != 0:
    raise Exception('Exception executing command %s' % ' '.join(cmd))


class WprManager:
  """A utility to download a WPR archive, host it, and forward device ports to
  it.
  """

  _WPR_BUCKET = 'chrome-partner-telemetry'

  def __init__(self, wpr_archive, device, cmdline_file, package):
    self._device = device
    self._wpr_archive = wpr_archive
    self._wpr_archive_hash = wpr_archive + '.sha1'
    self._cmdline_file = cmdline_file
    self._wpr_server = None
    self._host_http_port = None
    self._host_https_port = None
    self._flag_changer = None
    self._package = package

  def Start(self):
    """Set up the device and host for WPR."""
    self.Stop()
    self._BringUpWpr()
    self._StartForwarder()

  def Stop(self):
    """Clean up the device and host's WPR setup."""
    self._StopForwarder()
    self._StopWpr()

  def __enter__(self):
    self.Start()

  def __exit__(self, unused_exc_type, unused_exc_val, unused_exc_tb):
    self.Stop()

  def _BringUpWpr(self):
    """Start the WPR server on the host and the forwarder on the device."""
    print('Starting WPR on host...')
    _DownloadFromCloudStorage(self._WPR_BUCKET, self._wpr_archive_hash)
    if binary_manager.NeedsInit():
      binary_manager.InitDependencyManager([])
    self._wpr_server = webpagereplay_go_server.ReplayServer(
        self._wpr_archive, '127.0.0.1', 0, 0, replay_options=[])
    ports = self._wpr_server.StartServer()
    self._host_http_port = ports['http']
    self._host_https_port = ports['https']

  def _StopWpr(self):
    """ Stop the WPR and forwarder."""
    print('Stopping WPR on host...')
    if self._wpr_server:
      self._wpr_server.StopServer()
      self._wpr_server = None

  def _StartForwarder(self):
    """Sets up forwarding of device ports to the host, and configures chrome
    to use those ports.
    """
    if not self._wpr_server:
      logging.warning('No host WPR server to forward to.')
      return
    print('Starting device forwarder...')
    forwarder.Forwarder.Map([(0, self._host_http_port),
                             (0, self._host_https_port)],
                            self._device)
    device_http = forwarder.Forwarder.DevicePortForHostPort(
        self._host_http_port)
    device_https = forwarder.Forwarder.DevicePortForHostPort(
        self._host_https_port)
    self._flag_changer = flag_changer.FlagChanger(
        self._device, self._cmdline_file)
    self._flag_changer.AddFlags([
        '--host-resolver-rules=MAP * 127.0.0.1,EXCLUDE localhost',
        '--testing-fixed-http-port=%s' % device_http,
        '--testing-fixed-https-port=%s' % device_https,

        # Allows to selectively avoid certificate errors in Chrome. Unlike
        # --ignore-certificate-errors this allows exercising the HTTP disk cache
        # and avoids re-establishing socket connections. The value is taken from
        # the WprGo documentation at:
        # https://github.com/catapult-project/catapult/blob/master/web_page_replay_go/README.md
        '--ignore-certificate-errors-spki-list=' +
        'PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=',

        # The flag --ignore-certificate-errors-spki-list (above) requires
        # specifying the profile directory, otherwise it is silently ignored.
        '--user-data-dir=/data/data/{}'.format(self._package)])

  def _StopForwarder(self):
    """Shuts down the port forwarding service."""
    if self._flag_changer:
      print('Restoring flags while stopping forwarder, but why?...')
      self._flag_changer.Restore()
      self._flag_changer = None
    print('Stopping device forwarder...')
    forwarder.Forwarder.UnmapAllDevicePorts(self._device)


class AndroidProfileTool:
  """A utility for generating orderfile profile data for chrome on android.

  Runs cygprofile_unittest found in output_directory, does profiling runs,
  and pulls the data to the local machine in output_directory/profile_data.
  """

  _DEVICE_PROFILE_DIR = '/data/local/tmp/chrome/orderfile'

  # Old profile data directories that used to be used. These are cleaned up in
  # order to keep devices tidy.
  _LEGACY_PROFILE_DIRS = ['/data/local/tmp/chrome/cyglog']

  TEST_URL = 'http://en.m.wikipedia.org/wiki/Science'
  _WPR_ARCHIVE = os.path.join(
      os.path.dirname(__file__), 'memory_top_10_mobile_000.wprgo')

  def __init__(self,
               output_directory: str,
               host_profile_root: str,
               use_wpr: bool,
               urls: List[str],
               simulate_user: bool,
               device: device_utils.DeviceUtils,
               debug=False,
               verbosity=0):
    """Constructor.

    Args:
      output_directory: Chrome build directory.
      host_profile_root: Where to store the profiles on the host.
      use_wpr: Whether to use Web Page Replay.
      urls: URLs to load. Have to be contained in the WPR archive if
                  use_wpr is True.
      simulate_user: Whether to simulate a user.
      device: Android device selected to be used to
                            generate orderfile.
      debug: Use simpler, non-representative debugging profile.
      verbosity: The number of -v to pass to telemetry calls.
    """
    assert device, 'Expected a valid device'
    self._device = device
    self._cygprofile_tests = os.path.join(
        output_directory, 'cygprofile_unittests')
    self._host_profile_root = host_profile_root
    self._use_wpr = use_wpr
    self._urls = urls
    self._simulate_user = simulate_user
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

  def RunCygprofileTests(self):
    """Run the cygprofile unit tests suite on the device.

    Returns:
      The exit code for the tests.
    """
    device_path = '/data/local/tmp/cygprofile_unittests'
    self._device.PushChangedFiles([(self._cygprofile_tests, device_path)])
    try:
      self._device.RunShellCommand(device_path, check_return=True)
    except (device_errors.CommandFailedError,
            device_errors.DeviceUnreachableError):
      # TODO(jbudorick): Let the exception propagate up once clients can
      # handle it.
      logging.exception('Failure while running cygprofile_unittests:')
      return 1
    return 0

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
    print('Executing {} in {}'.format(' '.join(command), root))
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
      print('Enabling root...')
      self._device.EnableRoot()
      # SELinux need to be in permissive mode, otherwise the process cannot
      # write the log files.
      print('Putting SELinux in permissive mode...')
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
    print('Setting command line flags for {}...'.format(package_info.package))
    changer = flag_changer.FlagChanger(
        self._device, package_info.cmdline_file)
    changer.AddFlags(['--no-sandbox', '--disable-fre'])
    return changer

  def _RestoreCommandLineFlags(self, changer):
    print('Restoring command line flags...')
    if changer:
      changer.Restore()

  def _SetUpDeviceFolders(self):
    """Creates folders on the device to store profile data."""
    print('Setting up device folders...')
    self._DeleteDeviceData()
    self._device.RunShellCommand(['mkdir', '-p', self._DEVICE_PROFILE_DIR],
                                 check_return=True)

  def _DeleteDeviceData(self):
    """Clears out profile storage locations on the device. """
    for profile_dir in [self._DEVICE_PROFILE_DIR] + self._LEGACY_PROFILE_DIRS:
      self._device.RunShellCommand(
          ['rm', '-rf', str(profile_dir)],
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
    print('Pulling profile data into {}...'.format(host_profile_dir))

    self._SetUpHostFolders(host_profile_dir)
    self._device.PullFile(self._DEVICE_PROFILE_DIR,
                          host_profile_dir,
                          timeout=300)

    # Temporary workaround/investigation: if (for unknown reason) 'adb pull' of
    # the directory 'orderfile' '.../Release/profile_data' produces
    # '...profile_data/orderfile/files' instead of the usual
    # '...profile_data/files', list the files deeper in the tree.
    files = []
    redundant_dir_root = os.path.basename(self._DEVICE_PROFILE_DIR)
    for root_file in os.listdir(host_profile_dir):
      if root_file == redundant_dir_root:
        profile_dir = os.path.join(host_profile_dir, root_file)
        files.extend(os.path.join(profile_dir, f)
                     for f in os.listdir(profile_dir))
      else:
        files.append(os.path.join(host_profile_dir, root_file))

    if len(files) == 0:
      raise NoProfileDataError('No profile data was collected')

    return files


def AddProfileCollectionArguments(parser):
  """Adds the profiling collection arguments to |parser|."""
  parser.add_argument(
      '--no-wpr', action='store_true', help='Don\'t use WPR.')
  parser.add_argument('--urls', type=str, help='URLs to load.',
                      default=[AndroidProfileTool.TEST_URL],
                      nargs='+')
  parser.add_argument(
      '--simulate-user', action='store_true', help='More realistic collection.')


def CreateArgumentParser():
  """Creates and return the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--adb-path', type=os.path.realpath,
      help='adb binary')
  parser.add_argument(
      '--apk-path', type=os.path.realpath, required=True,
      help='APK to profile')
  parser.add_argument(
      '--output-directory', type=os.path.realpath, required=True,
      help='Chromium output directory (e.g. out/Release)')
  parser.add_argument(
      '--trace-directory', type=os.path.realpath,
      help='Directory in which profile traces will be stored. '
           'Defaults to <output-directory>/profile_data')
  AddProfileCollectionArguments(parser)
  return parser


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()

  devil_chromium.Initialize(
      output_directory=args.output_directory, adb_path=args.adb_path)

  trace_directory = args.trace_directory
  if not trace_directory:
    trace_directory = os.path.join(args.output_directory, 'profile_data')
  devices = device_utils.DeviceUtils.HealthyDevices()
  assert devices, 'Expected at least one connected device'
  profiler = AndroidProfileTool(args.output_directory,
                                host_profile_root=trace_directory,
                                use_wpr=not args.no_wpr,
                                urls=args.urls,
                                simulate_user=args.simulate_user,
                                device=devices[0])
  profiler.CollectSystemHealthProfile(args.apk_path)
  return 0


if __name__ == '__main__':
  sys.exit(main())
