#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


vpython='vpython3'

if sys.platform == 'win32':
    vpython += '.bat'

subprocess.run([
    vpython,
    os.path.join(os.path.dirname(__file__), 'regenerate_internal_cts_html.py')
] + sys.argv[1:],
               check=True)
