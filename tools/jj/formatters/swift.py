#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

_FORMATTER = "third_party/depot_tools/swift-format"

# Swift formatter is only supported on macOS, so make it a no-op on other
# platforms.
if sys.platform != "darwin":
  shutil.copyfileobj(sys.stdin, sys.stdout)
else:
  os.execv(_FORMATTER, [_FORMATTER] + sys.argv[1:])
