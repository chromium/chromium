#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fetch_lib
import sys

PLATFORM = 'Win'
MIN_VERSION = 1477889

if __name__ == '__main__':
    sys.exit(fetch_lib.main(PLATFORM, MIN_VERSION))
