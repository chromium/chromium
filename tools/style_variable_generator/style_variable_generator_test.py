# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from css_generator import CSSStyleGenerator
from proto_generator import ProtoStyleGenerator, ProtoJSONStyleGenerator
from views_generator import ViewsStyleGenerator
import unittest


class BaseStyleGeneratorTest:
    def assertEqualToFile(self, value, filename):
        with open(filename) as f:
            contents = f.read()
            self.assertEqual(
                value, contents,
                '\n>>>>>\n%s<<<<<\n\ndoes not match\n\n>>>>>\n%s<<<<<' %
                (value, contents))

    def testColorTestJSON(self):
        self.generator.out_file_path = (
            'tools/style_variable_generator/colors_test_expected.h')
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)


class ViewsStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ViewsStyleGenerator()
        self.generator.AddJSONFileToModel('colors_test_palette.json5')
        self.generator.AddJSONFileToModel('colors_test.json5')
        self.expected_output_file = 'colors_test_expected.h'


class CSSStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFileToModel('colors_test_palette.json5')
        self.generator.AddJSONFileToModel('colors_test.json5')
        self.expected_output_file = 'colors_test_expected.css'


class ProtoStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoStyleGenerator()
        self.generator.AddJSONFileToModel('colors_test_palette.json5')
        self.generator.AddJSONFileToModel('colors_test.json5')
        self.expected_output_file = 'colors_test_expected.proto'


class ProtoJSONStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoJSONStyleGenerator()
        self.generator.AddJSONFileToModel('colors_test_palette.json5')
        self.generator.AddJSONFileToModel('colors_test.json5')
        # Add in a separate file which adds more colors to test_colors so we can
        # confirm we do not generate duplicate fields.
        self.generator.AddJSONFileToModel('additional_colors_test.json5')
        self.expected_output_file = 'colors_test_expected.protojson'


if __name__ == '__main__':
    unittest.main()
