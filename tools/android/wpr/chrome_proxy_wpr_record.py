#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Begins proxying requests from Chrome, recording a WPR replay file.

To use the proxy, run Chrome with the flags printed by this script.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import time

chrome_root = os.path.join(os.path.dirname(__file__), '../../..')
sys.path.insert(0, os.path.join(chrome_root, 'build/android'))
sys.path.insert(0, os.path.join(chrome_root, 'third_party/catapult/devil'))

from devil.android import device_utils
import pylib.utils.chrome_proxy_utils as chrome_proxy_utils

parser = argparse.ArgumentParser()
parser.add_argument('--device', required=True)
args = parser.parse_args()

wpr_record_path = tempfile.mkstemp(prefix='wprrecord', suffix='.wprgo')[1]

session = chrome_proxy_utils.ChromeProxySession()
session.wpr_record_mode = True
session.Start(device_utils.DeviceUtils(args.device), wpr_record_path)

print('Use these chrome flags:')
print(' '.join(session.GetFlags()))

print('Recording to', wpr_record_path)

# When this script exits, recording stops. Wait for one hour.
print('Press Ctrl+C to quit')
time.sleep(3600)
