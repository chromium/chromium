#!/usr/bin/env vpython
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility library for running a startup profile on an Android device.

Sets up a device for cygprofile, disables sandboxing permissions, and sets up
support for web page replay, device forwarding, and fake certificate authority
to make runs repeatable.
"""

from __future__ import print_function

import argparse
import logging
import os
import shutil
import subprocess
import sys
import time

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
    super(NoProfileDataError, self).__init__()
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


def _SimulateSwipe(device, x1, y1, x2, y2):
  """Simulates a swipe on a device from (x1, y1) to (x2, y2).

  Coordinates are in (device dependent) pixels, and the origin is at the upper
  left corner.
  The simulated swipe will take 300ms.

  Args:
    device: (device_utils.DeviceUtils) device to run the command on.
    x1, y1, x2, y2: (int) Coordinates.
  """
  args = [str(x) for x in (x1, y1, x2, y2)]
  device.RunShellCommand(['input', 'swipe'] + args)


class WprManager(object):
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


class AndroidProfileTool(object):
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

  def __init__(self, output_directory, host_profile_dir, use_wpr, urls,
               simulate_user, device, debug=False):
    """Constructor.

    Args:
      output_directory: (str) Chrome build directory.
      host_profile_dir: (str) Where to store the profiles on the host.
      use_wpr: (bool) Whether to use Web Page Replay.
      urls: (str) URLs to load. Have to be contained in the WPR archive if
                  use_wpr is True.
      simulate_user: (bool) Whether to simulate a user.
      device: (DeviceUtils) Android device selected to be used to
                            generate orderfile.
      debug: (bool) Use simpler, non-representative debugging profile.
    """
    assert device, 'Expected a valid device'
    self._device = device
    self._cygprofile_tests = os.path.join(
        output_directory, 'cygprofile_unittests')
    self._host_profile_dir = host_profile_dir
    self._use_wpr = use_wpr
    self._urls = urls
    self._simulate_user = simulate_user
    self._debug = debug
    self._SetUpDevice()
    self._pregenerated_profiles = None


  def SetPregeneratedProfiles(self, files):
    """Set pregenerated profiles.

    The pregenerated files will be returned as profile data instead of running
    an actual profiling step.

    Args:
      files: ([str]) List of pregenerated files.
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

  def CollectProfile(self, apk, package_info):
    """Run a profile and collect the log files.

    Args:
      apk: The location of the chrome apk to profile.
      package_info: A PackageInfo structure describing the chrome apk,
                    as from pylib/constants.

    Returns:
      A list of cygprofile data files.

    Raises:
      NoProfileDataError: No data was found on the device.
    """
    if self._pregenerated_profiles:
      logging.info('Using pregenerated profiles instead of running profile')
      logging.info('Profile files:\n%s', '\n'.join(self._pregenerated_profiles))
      return self._pregenerated_profiles
    self._device.adb.Logcat(clear=True)
    self._Install(apk)
    try:
      changer = self._SetChromeFlags(package_info)
      self._SetUpDeviceFolders()
      if self._use_wpr:
        with WprManager(self._WPR_ARCHIVE, self._device,
                        package_info.cmdline_file, package_info.package):
          self._RunProfileCollection(package_info, self._simulate_user)
      else:
        self._RunProfileCollection(package_info, self._simulate_user)
    except device_errors.CommandFailedError as exc:
      logging.error('Exception %s; dumping logcat', exc)
      for logcat_line in self._device.adb.Logcat(dump=True):
        logging.error(logcat_line)
      raise
    finally:
      self._RestoreChromeFlags(changer)

    data = self._PullProfileData()
    self._DeleteDeviceData()
    return data

  def CollectSystemHealthProfile(self, apk):
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
    self._RunCommand(['tools/perf/run_benchmark',
                      '--device={}'.format(self._device.serial),
                      '--browser=exact',
                      '--browser-executable={}'.format(apk),
                      profile_benchmark])
    data = self._PullProfileData()
    self._DeleteDeviceData()
    return data

  @classmethod
  def _RunCommand(cls, command):
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

  def _RunProfileCollection(self, package_info, simulate_user):
    """Runs the profile collection tasks.

    If |simulate_user| is True, then try to simulate a real user, with swiping.
    Also do a first load of the page instead of about:blank, in order to
    exercise the cache. This is not desirable with a page that only contains
    cachable resources, as in this instance the network code will not be called.

    Args:
      package_info: Which Chrome package to use.
      simulate_user: (bool) Whether to try to simulate a user interacting with
                     the browser.
    """
    initial_url = self._urls[0] if simulate_user else 'about:blank'
    # Start up chrome once with a page, just to get the one-off
    # activities out of the way such as apk resource extraction and profile
    # creation.
    self._StartChrome(package_info, initial_url)
    time.sleep(15)
    self._KillChrome(package_info)
    self._SetUpDeviceFolders()
    for url in self._urls:
      self._StartChrome(package_info, url)
      time.sleep(15)
      if simulate_user:
        # Down, down, up, up.
        _SimulateSwipe(self._device, 200, 700, 200, 300)
        _SimulateSwipe(self._device, 200, 700, 200, 300)
        _SimulateSwipe(self._device, 200, 700, 200, 1000)
        _SimulateSwipe(self._device, 200, 700, 200, 1000)
      time.sleep(30)
      self._AssertRunning(package_info)
      self._KillChrome(package_info)

  def Cleanup(self):
    """Delete all local and device files left over from profiling. """
    self._DeleteDeviceData()
    self._DeleteHostData()

  def _Install(self, apk):
    """Installs Chrome.apk on the device.

    Args:
      apk: The location of the chrome apk to profile.
    """
    print('Installing apk...')
    self._device.Install(apk)

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

  def _SetChromeFlags(self, package_info):
    print('Setting Chrome flags...')
    changer = flag_changer.FlagChanger(
        self._device, package_info.cmdline_file)
    changer.AddFlags(['--no-sandbox', '--disable-fre'])
    return changer

  def _RestoreChromeFlags(self, changer):
    print('Restoring Chrome flags...')
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

  def _StartChrome(self, package_info, url):
    print('Launching chrome...')
    self._device.StartActivity(
        intent.Intent(package=package_info.package,
                      activity=package_info.activity,
                      data=url,
                      extras={'create_new_tab': True}),
        blocking=True, force_stop=True)

  def _AssertRunning(self, package_info):
    assert self._device.GetApplicationPids(package_info.package), (
        'Expected at least one pid associated with {} but found none'.format(
            package_info.package))

  def _KillChrome(self, package_info):
    self._device.ForceStop(package_info.package)

  def _DeleteHostData(self):
    """Clears out profile storage locations on the host."""
    shutil.rmtree(self._host_profile_dir, ignore_errors=True)

  def _SetUpHostFolders(self):
    self._DeleteHostData()
    os.mkdir(self._host_profile_dir)

  def _PullProfileData(self):
    """Pulls the profile data off of the device.

    Returns:
      A list of profile data files which were pulled.

    Raises:
      NoProfileDataError: No data was found on the device.
    """
    print('Pulling profile data...')
    self._SetUpHostFolders()
    self._device.PullFile(self._DEVICE_PROFILE_DIR, self._host_profile_dir,
                          timeout=300)

    # Temporary workaround/investigation: if (for unknown reason) 'adb pull' of
    # the directory 'orderfile' '.../Release/profile_data' produces
    # '...profile_data/orderfile/files' instead of the usual
    # '...profile_data/files', list the files deeper in the tree.
    files = []
    redundant_dir_root = os.path.basename(self._DEVICE_PROFILE_DIR)
    for root_file in os.listdir(self._host_profile_dir):
      if root_file == redundant_dir_root:
        profile_dir = os.path.join(self._host_profile_dir, root_file)
        files.extend(os.path.join(profile_dir, f)
                     for f in os.listdir(profile_dir))
      else:
        files.append(os.path.join(self._host_profile_dir, root_file))

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

  apk = apk_helper.ApkHelper(args.apk_path)
  package_info = None
  for p in constants.PACKAGE_INFO.itervalues():
    if p.package == apk.GetPackageName():
      package_info = p
      break
  else:
    raise Exception('Unable to determine package info for %s' % args.apk_path)

  trace_directory = args.trace_directory
  if not trace_directory:
    trace_directory = os.path.join(args.output_directory, 'profile_data')
  devices = device_utils.DeviceUtils.HealthyDevices()
  assert devices, 'Expected at least one connected device'
  profiler = AndroidProfileTool(
      args.output_directory, host_profile_dir=trace_directory,
      use_wpr=not args.no_wpr, urls=args.urls, simulate_user=args.simulate_user,
      device=devices[0])
  profiler.CollectProfile(args.apk_path, package_info)
  return 0


if __name__ == '__main__':
  sys.exit(main())
