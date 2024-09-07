#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import os
import unittest
import idl_schema
import json_parse
import model
from ts_definition_generator import TsDefinitionGenerator


class TsDefinitionGeneratorTest(unittest.TestCase):

  def _GetNamespace(self, fake_content, filename):
    """Returns a namespace object for the given content"""
    is_idl = filename.endswith('.idl')
    api_def = (idl_schema.Process(fake_content, filename)
               if is_idl else json_parse.Parse(fake_content))
    m = model.Model()
    return m.AddNamespace(api_def[0], filename)

  def LoadFile(self, rel_path: str):
    dir_path = os.path.dirname(__file__)
    resolved = os.path.join(dir_path, rel_path)
    text = ''
    with open(resolved, 'r') as file:
      text = file.read()
    return text

  def setUp(self):
    self.maxDiff = None  # Lets us see the full diff when inequal.

  def _runTest(self, in_file: str, expected_file):
    input_text = self.LoadFile(in_file)
    is_idl = in_file.endswith('.idl')
    file_name = 'file' + ('.idl' if is_idl else '.json')
    namespace = self._GetNamespace(input_text, file_name)
    result = TsDefinitionGenerator().Generate(namespace).Render()
    expected = self.LoadFile(expected_file)
    self.assertMultiLineEqual(expected, result)

  def testIdlBasics(self):
    self._runTest('test/idl_basics.idl', 'test/idl_basics_expected.d.ts')

  def testSimpleJsonApi(self):
    self._runTest('test/json_basics.json', 'test/json_basics_expected.d.ts')


if __name__ == '__main__':
  unittest.main()
