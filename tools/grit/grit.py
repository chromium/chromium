#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bootstrapping for GRIT.
'''


import os
import sys

import grit.grit_runner

sys.path.append(
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'diagnosis'))
try:
  import crbug_1001171
except ImportError:
  crbug_1001171 = None


if __name__ == '__main__':
  if crbug_1001171:
    with crbug_1001171.DumpStateOnLookupError():
      sys.exit(grit.grit_runner.Main(sys.argv[1:]))
  else:
    sys.exit(grit.grit_runner.Main(sys.argv[1:]))
