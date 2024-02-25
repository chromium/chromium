#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

_HERE_PATH = os.path.dirname(__file__)
sys.path.append(os.path.join(_HERE_PATH, '..', '..'))
sys.path.append(os.path.join(_HERE_PATH, '..'))

from resources import svgo_presubmit
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile

_OPTIMIZED_SVG = (
    b'<svg xmlns="http://www.w3.org/2000/svg" id="EXPORT_preserved_id" ' +
    b'width="24" height="24" fill="#757575" viewBox="0 0 24 24"><path ' +
    b'd="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 ' +
    b'2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/></svg>')

_UNOPTIMIZED_SVG = (b'''
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"
  viewBox="0 0 24 24" fill="#757575">
  <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 ''' +
                    b'''2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path>
</svg>
''')

_UNOPTIMIZED_IDS_SVG = (
    b'<svg xmlns="http://www.w3.org/2000/svg" id="stripped_id"><path d="M10 ' +
    b'4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 ' +
    b'2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/></svg>')


class SvgPresubmitTest(unittest.TestCase):
  def tearDown(self):
    os.remove(self._tmp_file)

  def check_contents(self, file_contents):
    tmp_args = {'suffix': '.svg', 'dir': _HERE_PATH, 'delete': False}
    with tempfile.NamedTemporaryFile(**tmp_args) as f:
      self._tmp_file = f.name
      f.write(file_contents)

    input_api = MockInputApi()
    input_api.files = [
        MockFile(os.path.abspath(self._tmp_file), file_contents.splitlines())
    ]
    input_api.presubmit_local_path = _HERE_PATH

    return svgo_presubmit.CheckOptimized(input_api, MockOutputApi())

  def assert_unoptimized_svg(self, file_contents):
    results = self.check_contents(file_contents)
    self.assertEqual(len(results), 1)
    self.assertTrue(results[0].type == 'notify')
    self.assertTrue('svgo' in results[0].message)

  def testUnoptimizedSvg(self):
    self.assert_unoptimized_svg(_UNOPTIMIZED_SVG)

  def testUnoptimizedIdsSVG(self):
    self.assert_unoptimized_svg(_UNOPTIMIZED_IDS_SVG)

  def testOptimizedSvg(self):
    self.assertEqual(len(self.check_contents(_OPTIMIZED_SVG)), 0)


if __name__ == '__main__':
  unittest.main()
