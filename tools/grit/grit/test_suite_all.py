#!/usr/bin/env python3
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test suite that collects all test cases for GRIT.'''


import os
import sys

CUR_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.normpath(os.path.join(CUR_DIR, '..', '..', '..'))
sys.path.insert(0, os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ'))

import typ


def main(args):
  return typ.main(top_level_dirs=[os.path.join(CUR_DIR, '..')],
                  skip=['grit.pseudo_unittest.*'])

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
