#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import svgo_presubmit
import sys
import tempfile
import unittest


_HERE_PATH = os.path.dirname(__file__)
sys.path.append(os.path.join(_HERE_PATH, '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile

_OPTIMIZED_SVG = '''
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="#757575"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/></svg>
'''.strip()

_UNOPTIMIZED_SVG = '''
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="#757575">
  <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path>
</svg>
'''

class SvgPresubmitTest(unittest.TestCase):
  def tearDown(self):
    os.remove(self._tmp_file)

  def check_contents(self, file_contents):
    tmp_args = {'suffix': '.svg', 'dir': _HERE_PATH, 'delete': False}
    with tempfile.NamedTemporaryFile(**tmp_args) as f:
      self._tmp_file = f.name
      f.write(file_contents)

    input_api = MockInputApi()
    input_api.files = [MockFile(os.path.abspath(self._tmp_file), file_contents.splitlines())]
    input_api.presubmit_local_path = _HERE_PATH

    return svgo_presubmit.CheckOptimized(input_api, MockOutputApi())

  def testUnoptimizedSvg(self):
    results = self.check_contents(_UNOPTIMIZED_SVG)
    self.assertEquals(len(results), 1)
    self.assertTrue(results[0].type == 'notify')
    self.assertTrue('svgo' in results[0].message)

  def testOptimizedSvg(self):
    self.assertEquals(len(self.check_contents(_OPTIMIZED_SVG)), 0)


if __name__ == '__main__':
  unittest.main()
