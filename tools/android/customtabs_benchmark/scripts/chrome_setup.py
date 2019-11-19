# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess


# Command line arguments for Chrome for loading performance measurements.
CHROME_ARGS = [
    # Disable backgound network requests that may pollute WPR archive, pollute
    # HTTP cache generation, and introduce noise in loading performance.
    '--disable-background-networking',
    '--disable-default-apps',
    '--no-proxy-server',

    # Disables actions that chrome performs only on first run or each launches,
    # which can interfere with page load performance, or even block its
    # execution by waiting for user input.
    '--disable-fre',
    '--no-default-browser-check',
    '--no-first-run',
]


def ResetChromeLocalState(device, package):
  """Remove the Chrome Profile and the various disk caches."""
  profile_dirs = ['app_chrome/Default', 'cache', 'app_chrome/ShaderCache',
                  'app_tabs']
  cmd = ['rm', '-rf']
  cmd.extend(
      '/data/data/{}/{}'.format(package, d) for d in profile_dirs)
  device.adb.Shell(subprocess.list2cmdline(cmd))
