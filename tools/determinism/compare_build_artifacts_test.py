#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import unittest
import os

import compare_build_artifacts


class Test(unittest.TestCase):
  def test_diff_binary(self):
    with tempfile.TemporaryDirectory() as dir:
      filea = os.path.join(dir, 'a')
      fileb = os.path.join(dir, 'b')

      with open(filea, 'wb') as f:
        f.write(b'a')

      with open(fileb, 'wb') as f:
        f.write(b'b')

      self.assertEqual(
          compare_build_artifacts.diff_binary(filea, fileb, 1),
          """1 out of 1 bytes are different (100.00%)
  0x0       : 61 'a'
              62 'b'
              62 'b""")


if __name__ == '__main__':
  unittest.main()
