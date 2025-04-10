#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bootstrapping for GRIT.
'''


import os
import sys

import grit.grit_runner


if __name__ == '__main__':
  sys.exit(grit.grit_runner.Main(sys.argv[1:]))
