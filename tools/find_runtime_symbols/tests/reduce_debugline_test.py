#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import textwrap
import unittest

from six import StringIO

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import reduce_debugline


class ReduceDebuglineTest(unittest.TestCase):
  _DECODED_DEBUGLINE = textwrap.dedent("""\
      Decoded dump of debug contents of section .debug_line:

      CU: ../../chrome/service/service_main.cc:
      File name                            Line number    Starting address
      service_main.cc                               21            0xa41210

      service_main.cc                               24            0xa4141f
      service_main.cc                               30            0xa4142b
      service_main.cc                               31            0xa4143e

      ../../base/message_loop.h:
      message_loop.h                               550            0xa41300

      message_loop.h                               551            0xa41310

      ../../base/logging.h:
      logging.h                                    246            0xa41710

      logging.h                                    247            0xa41726

      ../../base/logging.h:
      logging.h                                    846            0xa3fd90

      logging.h                                    846            0xa3fda0

      """)

  _EXPECTED_REDUCED_DEBUGLINE = [
      (0xa3fd90, '../../base/logging.h'),
      (0xa41210, '../../chrome/service/service_main.cc'),
      (0xa41300, '../../base/message_loop.h'),
      (0xa4141f, '../../chrome/service/service_main.cc'),
      (0xa41710, '../../base/logging.h'),
      ]

  def test(self):
    ranges_dict = reduce_debugline.reduce_decoded_debugline(
        StringIO(self._DECODED_DEBUGLINE))
    self.assertEqual(self._EXPECTED_REDUCED_DEBUGLINE, ranges_dict)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
