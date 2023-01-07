#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Begins proxying requests from Chrome, replaying a WPR replay file.

To use the proxy, run Chrome with the flags printed by this script.
"""

import argparse
import os
import subprocess
import sys
import time

chrome_root = os.path.join(os.path.dirname(__file__), '../../..')
sys.path.insert(0, os.path.join(chrome_root, 'build/android'))
sys.path.insert(0, os.path.join(chrome_root, 'third_party/catapult/devil'))

from devil.android import device_utils
import pylib.utils.chrome_proxy_utils as chrome_proxy_utils

parser = argparse.ArgumentParser()
parser.add_argument('--device', required=True)
parser.add_argument('--replay-file', required=True)
args = parser.parse_args()

session = chrome_proxy_utils.ChromeProxySession()
session.wpr_record_mode = False
session.Start(device_utils.DeviceUtils(args.device), args.replay_file)

print('Use these chrome flags:')
print(' '.join(session.GetFlags()))

print('Replaying ', args.replay_file)

# When this script exits, replaying stops. Wait for one hour.
print('Press Ctrl+C to quit')
time.sleep(3600)
