#!/usr/bin/env python3.8
# Copyright 2023 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Temporary script for soft transitioning https://fxbug.dev/39388."""

# TODO(fxbug.dev/39388): Remove this file after https://fxrev.dev/953989 rolls into chromium.

import subprocess
import sys

fidlgen_cmd = ["./" + sys.argv[1]] + sys.argv[2:-2]
assert sys.argv[-2] == "--tables"
# Write the file so it exists even before https://fxrev.dev/953989 rolls.
with open(sys.argv[-1], "w") as f:
    pass
sys.exit(subprocess.call(fidlgen_cmd))
