#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_unexpire_flags
import os
import unittest


class TestUnexpireGenerator(unittest.TestCase):
  TEST_MSTONE = 123

  def read_golden_file(self, extension):
    return file(
        os.path.join(
            os.path.dirname(__file__),
            'unexpire_test.' + extension + '.expected')).read()

  def testCcFile(self):
    cc = generate_unexpire_flags.gen_features_impl('foobar', 123)
    golden_cc = self.read_golden_file('cc')
    self.assertEquals(golden_cc, cc)

  def testHFile(self):
    h = generate_unexpire_flags.gen_features_header('foobar', 123)
    golden_h = self.read_golden_file('h')
    self.assertEquals(golden_h, h)

  def testIncFile(self):
    inc = generate_unexpire_flags.gen_flags_fragment('foobar', 123)
    golden_inc = self.read_golden_file('inc')
    self.assertEquals(golden_inc, inc)


if __name__ == '__main__':
  unittest.main()
