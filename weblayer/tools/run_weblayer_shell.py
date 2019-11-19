#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                                'build', 'android'))
import devil_chromium
from devil.android import device_utils

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--shell-apk-path', type=os.path.abspath, required=True,
                      help='Absolute path to the WebLayer shell APK to use.')
  parser.add_argument('--support-apk-path', nargs='+', type=os.path.abspath,
                      required=True,
                      help='Absolute path to the WebLayer support APKs to use.')
  parser.add_argument('--switch-webview-to', type=str, required=False,
                      help='Package name to set as the WebView implementation.')
  parser.add_argument('-d', '--device', dest='devices', action='append',
                      default=[],
                      help='Target device for apk to install on. Enter multiple'
                           ' times for multiple devices.')
  args = parser.parse_args()

  devil_chromium.Initialize()
  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.devices)

  def install(device):
    print 'Installing %s...' % args.shell_apk_path
    device.Install(args.shell_apk_path, reinstall=True, allow_downgrade=True)
    print 'Success'
    for path in args.support_apk_path:
      print 'Installing %s...' % path
      device.Install(path, reinstall=True, allow_downgrade=True)
      print 'Success'
    if args.switch_webview_to:
      print 'Setting WebView implementation to %s' % args.switch_webview_to
      device.SetWebViewImplementation(args.switch_webview_to)
      print 'Done'

    device.adb.Shell('monkey -p org.chromium.weblayer.shell 1')

  device_utils.DeviceUtils.parallel(devices).pMap(install)

if __name__ == '__main__':
  sys.exit(main())
