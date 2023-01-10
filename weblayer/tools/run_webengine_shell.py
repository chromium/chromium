#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                                'build', 'android'))
import devil_chromium
from devil.android import apk_helper
from devil.android import device_utils

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--shell-apk-path', type=os.path.abspath, required=True,
                      help='Absolute path to the WebEngine shell APK to use.')
  parser.add_argument('--support-apk-path', action='append',
                      type=os.path.abspath, default=[],
                      help='Absolute path to the WebLayer support APKs to '
                      'use. Specify multiple times for multiple paths.')
  parser.add_argument('--switch-webview-to', type=str, required=False,
                      help='APK to set as the WebView implementation.')
  parser.add_argument('-d', '--device', dest='devices', action='append',
                      default=[],
                      help='Target device for apk to install on. Enter multiple'
                           ' times for multiple devices.')
  parser.add_argument('remaining_args', nargs=argparse.REMAINDER,
                      help='Flags to be passed to WebLayer should be appended'
                           ' as --args="--myflag"')
  args = parser.parse_args()

  devil_chromium.Initialize()
  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.devices)

  def install(device):
    print('Installing %s...' % args.shell_apk_path)
    device.Install(args.shell_apk_path, reinstall=True, allow_downgrade=True)
    print('Success')
    for path in args.support_apk_path:
      print('Installing %s...' % path)
      device.Install(path, reinstall=True, allow_downgrade=True)
      print('Success')
    if args.switch_webview_to:
      print('Installing %s...' % args.switch_webview_to)
      device.Install(args.switch_webview_to, reinstall=True,
                     allow_downgrade=True)
      package = apk_helper.GetPackageName(args.switch_webview_to)
      print('Setting WebView implementation to %s' % package)
      device.SetWebViewImplementation(package)
      print('Done')

    if (os.path.basename(args.shell_apk_path) == "WEShellLocal.apk"):
      launch_cmd = [
        os.path.join(os.path.dirname(args.shell_apk_path),
                     os.pardir, 'bin', 'webengine_shell_local_apk'),
        'launch'
      ]
      launch_cmd.extend(args.remaining_args)
      subprocess.call(launch_cmd)
    elif (os.path.basename(args.shell_apk_path) == "WEShellSandbox.apk"):
      launch_cmd = [
        os.path.join(os.path.dirname(args.shell_apk_path),
                     os.pardir, 'bin', 'webengine_shell_sandbox_apk'),
        'launch'
      ]
      launch_cmd.extend(args.remaining_args)
      subprocess.call(launch_cmd)
    else:
      device.adb.Shell('monkey -p org.chromium.webengine.shell 1')

  device_utils.DeviceUtils.parallel(devices).pMap(install)

if __name__ == '__main__':
  sys.exit(main())
