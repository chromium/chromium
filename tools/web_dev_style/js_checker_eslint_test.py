#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import js_checker
import json
import os
import sys
import unittest
import tempfile


_HERE_PATH = os.path.dirname(__file__)
sys.path.append(os.path.join(_HERE_PATH, '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile


class JsCheckerEsLintTest(unittest.TestCase):
  def setUp(self):
    self._tmp_files = []

  def tearDown(self):
    for file in self._tmp_files:
      os.remove(file)

  def _runChecks(self, file_contents, file_type):
    tmp_args = {'suffix': '.' + file_type, 'dir': _HERE_PATH, 'delete': False}
    with tempfile.NamedTemporaryFile(**tmp_args) as f:
      tmp_file = f.name
      self._tmp_files.append(tmp_file)
      f.write(file_contents.encode('utf-8'))

    input_api = MockInputApi()
    input_api.files = [MockFile(os.path.abspath(tmp_file), '')]
    input_api.presubmit_local_path = _HERE_PATH

    checker = js_checker.JSChecker(input_api, MockOutputApi())

    try:
      # ESLint JSON warnings come from stdout without error return.
      output = checker.RunEsLintChecks(input_api.AffectedFiles(),
                                       format='json')[0]
      json_error = str(output)
    except RuntimeError as err:
      # Extract ESLint's JSON error output from the error message.
      message = str(err)
      json_error = message[message.index('['):]

    return json.loads(json_error)[0].get('messages')

  def _assertError(self, results, rule_id, line):
    self.assertEqual(1, len(results))
    message = results[0]
    self.assertEqual(rule_id, message.get('ruleId'))
    self.assertEqual(line, message.get('line'))

  def testGetElementByIdCheck(self):
    results = self._runChecks("const a = document.getElementById('foo');", 'js')
    self._assertError(results, 'no-restricted-properties', 1)

    results = self._runChecks(
        "const a: HTMLELement = document.getElementById('foo');", 'ts')
    self._assertError(results, 'no-restricted-properties', 1)

  def testPrimitiveWrappersCheck(self):
    results = self._runChecks('const a = new Number(1);', 'js')
    self._assertError(results, 'no-new-wrappers', 1)

    results = self._runChecks('const a: number = new Number(1);', 'ts')
    self._assertError(results, 'no-new-wrappers', 1)

  def testTypeScriptEslintPluginCheck(self):
    results = self._runChecks('const a: any;', 'ts')
    self._assertError(results, '@typescript-eslint/no-explicit-any', 1)

    results = self._runChecks('const a: number = 1;', 'ts')
    self._assertError(results, '@typescript-eslint/no-inferrable-types', 1)


if __name__ == '__main__':
  unittest.main()
