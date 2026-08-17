#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.PRESUBMIT as PRESUBMIT
import chromium_src.PRESUBMIT_test_mocks as PRESUBMIT_test_mocks


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
