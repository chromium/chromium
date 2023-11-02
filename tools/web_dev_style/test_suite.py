#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

subprocess.check_call([
    sys.executable, '-m', 'unittest', 'discover', '-p', '*test.py', '-t', '..'
])
