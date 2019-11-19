# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from core.results_processor import util


class UtilTests(unittest.TestCase):
  def testApplyInParallel(self):
    work_list = [[1], [2], [3]]
    def fun(x):
      x.extend(x)
    util.ApplyInParallel(fun, work_list)
    self.assertEqual(work_list, [[1, 1], [2, 2], [3, 3]])

  def testApplyInParallelOnFailure(self):
    work_list = [[1], [2], [3]]
    def fun(x):
      if x == [3]:
        raise RuntimeError()
    util.ApplyInParallel(fun, work_list, on_failure=lambda x: x.pop())
    self.assertEqual(work_list, [[1], [2], []])
