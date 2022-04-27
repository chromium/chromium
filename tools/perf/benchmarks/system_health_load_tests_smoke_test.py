#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to check story version in system_health_smoke_tests."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from core import path_util
path_util.AddTelemetryToPath()

from benchmarks import system_health_smoke_test

def main():
  system_health_smoke_test.validate_smoke_test_name_versions()
  return 0

if __name__ == '__main__':
  sys.exit(main())
