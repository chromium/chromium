#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.python_support.script_checker as script_checker
from chromium_src.tools.metrics.python_support.tests_helpers import (
  TestableScript,
)

# Fails on purpose, so that check_scripts() returns what the script printed.
_ECHO_STDIN_AND_FAIL = """
import sys
sys.stdout.write(sys.stdin.read())
sys.exit(1)
"""


class CheckScriptsTest(unittest.TestCase):
  def testPassesStdinToTheScript(self):
    lines = [f'tools/metrics/dir{i}/file{i}.py' for i in range(10000)]
    with tempfile.TemporaryDirectory() as temp_dir:
      script_path = pathlib.Path(temp_dir, 'echo_stdin.py')
      script_path.write_text(_ECHO_STDIN_AND_FAIL)
      failed = script_checker.check_scripts(
        [
          TestableScript(
            identifiable_name='echo_stdin',
            file_path=script_path,
            cmd=[sys.executable, str(script_path)],
            stdin='\n'.join(lines),
          )
        ],
        cwd=temp_dir,
        display_progressbar=False,
      )
    self.assertEqual([r.stdout.splitlines() for r in failed], [lines])


class ReadStdinLinesTest(unittest.TestCase):
  def testReadsUtf8Lines(self):
    stdin_bytes = 'a.py\r\nb_\u017c.py\n'.encode('utf-8')
    with mock.patch.object(
      sys, 'stdin', io.TextIOWrapper(io.BytesIO(stdin_bytes))
    ):
      self.assertEqual(
        script_checker.read_stdin_lines(), ['a.py', 'b_\u017c.py']
      )


if __name__ == '__main__':
  unittest.main()
