#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
sys.path += [os.path.dirname(os.path.dirname(__file__))]

from json_data_generator.generator import JSONDataGenerator
import unittest


class JSONDataGeneratorTest(unittest.TestCase):
    def assertEqualToFile(self, value, filename):
        with open(filename, 'r') as f:
            self.maxDiff = None
            self.assertEqual(value, f.read())

    def setUp(self):
        self.generator = JSONDataGenerator('test')
        self.generator.AddJSONFilesToModel(
            ['test/test_data1.json5', 'test/test_data2.json5'])

    def testFileGeneration(self):
        generated_content = self.generator.RenderTemplate(
            'test/template.test.jinja', 'test/jinja_helper.py')
        self.assertEqualToFile(generated_content, 'test/expected.generated')


if __name__ == '__main__':
    unittest.main()
