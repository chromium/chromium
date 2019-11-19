#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test suite that collects all test cases for GRIT.'''

from __future__ import print_function

import os
import sys


CUR_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(CUR_DIR)))
TYP_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if TYP_DIR not in sys.path:
    sys.path.insert(0, TYP_DIR)


import typ  # pylint: disable=import-error,unused-import


def main(args):
    return typ.main(
      top_level_dirs=[os.path.join(CUR_DIR, '..')],
      skip=['grit.format.gen_predetermined_ids_unittest.*',
            'grit.pseudo_unittest.*']
      )

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
