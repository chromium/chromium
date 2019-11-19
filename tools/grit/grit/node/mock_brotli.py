#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mock Brotli Executable for testing purposes."""

import sys

sys.stdout.write('This has been mock compressed!')
