#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
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

    output = checker.RunEsLintChecks(input_api.AffectedFiles(),
                                     format='json')[0]

    # Extract ESLint's error from the PresubmitError. This is added in
    # third_party/node/node.py.
    search_token = '\' failed\n'
    json_start_index = output.message.index(search_token)
    json_error_str = output.message[json_start_index + len(search_token):]
    # ESLint's errors are in JSON format.
    return json.loads(json_error_str)[0].get('messages')

  def _assertError(self, results, rule_id, line):
    self.assertEqual(1, len(results))
    message = results[0]
    self.assertEqual(rule_id, message.get('ruleId'))
    self.assertEqual(line, message.get('line'))

  def testPrimitiveWrappersCheck(self):
    results = self._runChecks('const a = new Number(1);', 'js')
    self._assertError(results, 'no-new-wrappers', 1)

    results = self._runChecks(
        '''
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    const a: number = new Number(1);
''', 'ts')
    self._assertError(results, 'no-new-wrappers', 3)

  def testTypeScriptEslintPluginCheck(self):
    results = self._runChecks('const a: number = 1;', 'ts')
    self._assertError(results, '@typescript-eslint/no-unused-vars', 1)


if __name__ == '__main__':
  unittest.main()
