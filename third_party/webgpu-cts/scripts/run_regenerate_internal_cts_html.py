#!/usr/bin/env python
# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import subprocess
import sys

# We need a small shim because Windows needs a batch file to execute.
if platform.system() == 'Windows':
    script = 'regenerate_internal_cts_html.bat'
else:
    script = 'regenerate_internal_cts_html.py'

process = subprocess.Popen(
    [os.path.join(os.path.dirname(os.path.abspath(__file__)), script)] +
    sys.argv[1:])
process.communicate()
sys.exit(process.returncode)
