#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import tempfile
import unittest

import pak_util


class PackUtilTest(unittest.TestCase):
  def test_extract(self):
    temp = tempfile.TemporaryDirectory()
    old_argv = sys.argv
    sys.argv = [
        'pak_util_unittest.py', 'extract', 'grit/testdata/resources.pak', '-o',
        temp.name
    ]
    pak_util.main()
    sys.argv = old_argv
