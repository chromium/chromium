#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bootstrapping for GRIT.
'''

from __future__ import print_function

import os
import sys

import grit.grit_runner

sys.path.append(os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', 'diagnosis')))
import crbug_1001171


if __name__ == '__main__':
  with crbug_1001171.DumpStateOnLookupError():
    sys.exit(grit.grit_runner.Main(sys.argv[1:]))
