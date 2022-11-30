#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test suite that collects all test cases for Polymer.'''

import os
import sys

CUR_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(CUR_DIR))
TYP_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if TYP_DIR not in sys.path:
  sys.path.insert(0, TYP_DIR)

import typ


def main(args):
  os.chdir(CUR_DIR)
  return typ.main(top_level_dirs=[CUR_DIR])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
