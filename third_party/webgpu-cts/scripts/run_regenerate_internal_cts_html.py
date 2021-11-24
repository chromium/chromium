#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


subprocess.run([
    'vpython3',
    os.path.join(os.path.dirname(__file__), 'regenerate_internal_cts_html.py')
] + sys.argv[1:],
               check=True)
