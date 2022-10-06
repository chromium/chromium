#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def PathToNyc():
  return os.path.join(
      os.path.dirname(__file__), 'node_modules', 'nyc', 'bin', 'nyc')
