#!/usr/bin/env python3.8
# Copyright 2019 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script for GN to run an arbitrary binary.

Run with:
  python3.8 gn_run_binary.py <binary_name> [args ...]
"""

import os
import subprocess
import sys

# This script is designed to run binaries produced by the current build. We
# may prefix it with "./" to avoid picking up system versions that might
# also be on the path.
path = sys.argv[1]
if not os.path.isabs(path):
    path = './' + path

# The rest of the arguments are passed directly to the executable.
args = [path] + sys.argv[2:]

sys.exit(subprocess.call(args))
