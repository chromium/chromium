#!/usr/bin/env python
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import codegen_shared


class CodegenSharedTest(unittest.TestCase):
  def testHash(self) -> None:
    # Must match those in //base/metrics/metrics_hashes_unittest.cc.
    self.assertEqual(codegen_shared.HashName('Back'), 0x0557fa923dcee4d0)
    self.assertEqual(codegen_shared.HashName('Forward'), 0x67d2f6740a8eaebf)
    self.assertEqual(codegen_shared.HashName('NewTab'), 0x290eb683f96572f1)


if __name__ == '__main__':
  unittest.main()
