#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import sys
import unittest

import package


class TeeCmdTest(unittest.TestCase):
  def testKeepsAllOutput(self):
    cmd = [sys.executable, '-c', 'for i in range(5000): print(i)']
    log = io.StringIO()
    stdout = io.StringIO()
    with contextlib.redirect_stdout(stdout):
      package.TeeCmd(cmd, log)
    expected = [str(i) for i in range(5000)]
    self.assertEqual(log.getvalue().splitlines(), expected)
    self.assertEqual(stdout.getvalue().splitlines(), expected)

  def testExitsIfCommandFails(self):
    cmd = [sys.executable, '-c', "import sys; print('error'); sys.exit(3)"]
    log = io.StringIO()
    with contextlib.redirect_stdout(io.StringIO()):
      with self.assertRaises(SystemExit):
        package.TeeCmd(cmd, log)
    self.assertEqual(log.getvalue().splitlines(), ['error'])


if __name__ == '__main__':
  unittest.main()
