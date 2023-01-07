# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from core import path_util


class PathUtilTest(unittest.TestCase):

  def testSysPath(self):
    sys_path_before = list(sys.path)
    with path_util.SysPath('_test_dir'):
      sys_path_within_context = list(sys.path)
    sys_path_after = list(sys.path)

    self.assertEqual(sys_path_before, sys_path_after)
    self.assertEqual(sys_path_before + ['_test_dir'], sys_path_within_context)
