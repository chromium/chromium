# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Helper script to find the Windows SDK directory.
"""

from __future__ import print_function

import os
import sys

# Add to the Python path so that we can import vs_toolchain
script_path = os.path.abspath(os.path.dirname(__file__))
build_path = os.path.normpath(os.path.join(script_path, '..', '..', 'build'))
sys.path.append(build_path)

import vs_toolchain

# Print the Windows SDK directory, either a local install or the packaged
# toolchain.
print(vs_toolchain.SetEnvironmentAndGetSDKDir())
