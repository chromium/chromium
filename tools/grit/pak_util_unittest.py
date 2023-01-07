#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import unittest

import pak_util


class PackUtilTest(unittest.TestCase):
  def test_extract(self):
    tempdir = tempfile.mkdtemp()
    old_argv = sys.argv
    grit_root_dir = os.path.abspath(os.path.dirname(__file__))
    sys.argv = [
        'pak_util_unittest.py', 'extract',
        os.path.join(grit_root_dir, 'grit/testdata/resources.pak'), '-o',
        tempdir
    ]
    pak_util.main()
    sys.argv = old_argv
    shutil.rmtree(tempdir, ignore_errors=True)
