#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

TESTS = [
    'compression_script_test',
    'elf_headers_test',
]

if __name__ == '__main__':
  suite = unittest.TestSuite()
  suite.addTests(unittest.defaultTestLoader.loadTestsFromNames(TESTS))
  unittest.TextTestRunner().run(suite)
