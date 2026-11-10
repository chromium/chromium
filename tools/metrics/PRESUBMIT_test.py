#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys
import unittest
from typing import List

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.PRESUBMIT as PRESUBMIT
import chromium_src.PRESUBMIT_test_mocks as PRESUBMIT_test_mocks

# Windows runs vpython3 through cmd.exe, which rejects longer command lines.
_CMD_EXE_COMMAND_LINE_LIMIT = 8191


def _AffectedFiles(count: int) -> List[str]:
  return [f'tools/metrics/dir{i}/file{i}.py' for i in range(count)]


class PresubmitChecksAsTestableScriptsTest(unittest.TestCase):
  def testCommandLinesDoNotDependOnAffectedFiles(self):
    few = PRESUBMIT._PresubmitChecksAsTestableScripts(_AffectedFiles(10))
    many = PRESUBMIT._PresubmitChecksAsTestableScripts(_AffectedFiles(10000))
    self.assertEqual([s.cmd for s in few], [s.cmd for s in many])
    self.assertLess(
      max(len(' '.join(s.cmd)) for s in many), _CMD_EXE_COMMAND_LINE_LIMIT
    )

  def testAffectedFilesArePassedOnStdin(self):
    affected_files = _AffectedFiles(10000)
    scripts = PRESUBMIT._PresubmitChecksAsTestableScripts(affected_files)
    self.assertEqual(
      {s.file_path: s.stdin.splitlines() for s in scripts if s.stdin},
      {
        pathlib.Path(
          'tools/metrics/python_support/validate_affected_scripts.py'
        ): affected_files,
        pathlib.Path('tools/metrics/metrics_python_tests.py'): affected_files,
      },
    )


class CheckQuoteConsistencyTest(unittest.TestCase):
  def _RunQuoteCheck(self, code_text):
    input_api = PRESUBMIT_test_mocks.MockInputApi()
    input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'tools/metrics/test_file.py',
          code_text.splitlines(),
        )
      ]
    )
    return PRESUBMIT._CheckQuoteConsistency(
      input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )

  def testSingleQuotesAllowed(self):
    results = self._RunQuoteCheck(
      "x = 'hello'\ny = f'hello {name}'\nz = 'don\\'t'\n"
    )
    self.assertEqual(len(results), 0)

  def testTripleQuotesAllowed(self):
    results = self._RunQuoteCheck(
      '"""docstring"""\n'
      'x = """multiline string"""\n'
      "y = '''another docstring'''\n"
    )
    self.assertEqual(len(results), 0)

  def testDoubleQuotesWithSingleQuotesAllowed(self):
    results = self._RunQuoteCheck('x = "don\'t"\ny = f"hello \'{name}\'"\n')
    self.assertEqual(len(results), 0)

  def testDoubleQuotesWithoutSingleQuotesFlagged(self):
    results = self._RunQuoteCheck('x = "hello"\ny = f"hello {name}"\n')
    self.assertEqual(len(results), 2)
    self.assertIn('test_file.py:1 uses double quotes', results[0].message)
    self.assertIn('test_file.py:2 uses double quotes', results[1].message)

  def testImplicitConcatenationWithSingleQuotesAllowed(self):
    results = self._RunQuoteCheck(
      'x = "hello " "world\'s"\n'
      'y = (\n'
      '    f"Expiry {name} does not match expected format "\n'
      "    f\"('{EXPIRY_DATE_PATTERN}') or 'never': \"\n"
      '    f"found {expiry_str}"\n'
      ')\n'
    )
    self.assertEqual(len(results), 0)

  def testImplicitConcatenationWithoutSingleQuotesFlagged(self):
    results = self._RunQuoteCheck(
      'x = "hello " "world"\n'
      'y = (\n'
      '    f"Expiry {name} does not match expected format "\n'
      '    f"or never: "\n'
      '    f"found {expiry_str}"\n'
      ')\n'
    )
    # Should report one violation for the first line of each concatenated group
    self.assertEqual(len(results), 2)
    self.assertIn('test_file.py:1 uses double quotes', results[0].message)
    self.assertIn('test_file.py:3 uses double quotes', results[1].message)

  def testEscapedDoubleQuotesAllowed(self):
    results = self._RunQuoteCheck('x = \'hello \\"world\\"\'\n')
    self.assertEqual(len(results), 0)


if __name__ == '__main__':
  unittest.main()
