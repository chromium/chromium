#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
from pathlib import Path

if len(Path(__file__).parents) > 2:
    sys.path += [str(Path(__file__).parents[2])]

from style_variable_generator.model import Modes
from style_variable_generator.css_generator import CSSStyleGenerator
from style_variable_generator.proto_generator import ProtoStyleGenerator, ProtoJSONStyleGenerator
from style_variable_generator.json_generator import JSONStyleGenerator
from style_variable_generator.views_generator import ViewsHStyleGenerator, ViewsCCStyleGenerator
from style_variable_generator.ts_generator import TSStyleGenerator
from style_variable_generator.color_mappings_generator import ColorMappingsHStyleGenerator, ColorMappingsCCStyleGenerator
import unittest

print(os.path.join(os.path.dirname(__file__)))

class BaseStyleGeneratorTest:
    def assertEqualToFile(self, value, filename):
        path = os.path.join(os.path.dirname(__file__), 'goldens', filename)
        with open(path, 'r') as f:
            self.maxDiff = None
            self.assertEqual(value, f.read(), f'Did not match golden: {path}')

    def AddJSONFilesToModel(self, files):
        relpaths_from_cwd = [
            os.path.relpath(os.path.join(os.path.dirname(__file__), f),
                            os.getcwd()).replace('\\', '/') for f in files
        ]
        self.generator.AddJSONFilesToModel(relpaths_from_cwd)


class ViewsStyleHGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ViewsHStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.h.generated'

    def testColorTestJSON(self):
        self.generator.out_file_path = (
            'tools/style_variable_generator/colors_test_expected.h')
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testTokenStyleNames(self):
        self.generator = ViewsHStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        self.expected_output_file = 'colors_tokens_test_expected.h.generated'


class ViewsStyleCCGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ViewsCCStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.cc.generated'

    def testColorTestJSON(self):
        self.generator.out_file_path = (
            'tools/style_variable_generator/colors_test_expected.h')
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testTokenStyleNames(self):
        self.generator = ViewsCCStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        self.expected_output_file = 'colors_tokens_test_expected.cc.generated'


class CSSStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.css'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testCustomDarkModeSelector(self):
        expected_file_name = 'colors_test_custom_dark_toggle_expected.css'
        self.generator.generator_options = {
            'dark_mode_selector': 'html[dark]:not(body)'
        }
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testCustomDarkModeOnly(self):
        expected_file_name = 'colors_test_dark_only_expected.css'
        self.generator.generate_single_mode = Modes.DARK
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testUntypedCSS(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(['untyped_css_test.json5'])
        expected_file_name = 'untyped_css_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypography(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(['typography_test.json5'])
        expected_file_name = 'typography_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testSuppressSourcesComment(self):
        self.generator.generator_options = {'suppress_sources_comment': 'true'}
        expected_file_name = 'suppress_sources_comment_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTokenStyleNames(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        expected_file_name = 'colors_tokens_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testLegacyColors(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(['legacy_mappings_test.json5'])
        expected_file_name = 'legacy_mappings_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class TSStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = TSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.ts'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testIncludeStyleSheet(self):
        expected_file_name = 'colors_test_include_style_sheet_expected.ts'
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypography(self):
        expected_file_name = 'colors_test_typography_expected.ts'
        self.AddJSONFilesToModel(['typography_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testUntypedCSS(self):
        expected_file_name = 'colors_test_untyped_css_expected.ts'
        self.AddJSONFilesToModel(['untyped_css_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypographyAndUntypedCSS(self):
        expected_file_name = (
            'colors_test_typography_and_untyped_css_expected.ts')
        self.AddJSONFilesToModel(
            ['typography_test.json5', 'untyped_css_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testSuppressSourcesComment(self):
        self.generator.generator_options = {'suppress_sources_comment': 'true'}
        expected_file_name = 'suppress_sources_comment_test_expected.ts'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTokenStyleNames(self):
        self.generator = TSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        expected_file_name = 'colors_tokens_test_expected.ts'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class JSONStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = JSONStyleGenerator()
        paths = [
            'colors_test_palette.json5',
            'colors_test.json5',
        ]
        self.AddJSONFilesToModel(paths)
        self.expected_output_file = 'colors_test_expected.json'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testTokenStyleNames(self):
        self.generator = JSONStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        expected_file_name = 'colors_tokens_test_expected.json'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class ProtoStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.proto'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testTokenStyleNames(self):
        self.generator = ProtoStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        expected_file_name = 'colors_tokens_test_expected.proto'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class ProtoJSONStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoJSONStyleGenerator()
        paths = [
            'colors_test_palette.json5',
            'colors_test.json5',
            # Add in a separate file which adds more colors to test_colors so we
            # can confirm we do not generate duplicate fields.
            'additional_colors_test.json5',
        ]
        self.AddJSONFilesToModel(paths)
        self.expected_output_file = 'colors_test_expected.protojson'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)

    def testTokenStyleNames(self):
        self.generator = ProtoJSONStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        expected_file_name = 'colors_tokens_test_expected.protojson'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class ColorMappingsStyleGeneratorTest(unittest.TestCase,
                                      BaseStyleGeneratorTest):
    def setUp(self):
        self.generator_options = {
            'cpp_namespace': 'test_tokens',
            'color_id_start_value': '0xF000'
        }

    def testColorMappingsCC(self):
        self.generator = ColorMappingsCCStyleGenerator()
        self.generator.generator_options = self.generator_options
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        self.assertEqualToFile(
            self.generator.Render(),
            'colors_tokens_test_color_mappings.cc.generated')

    def testColorMappingsH(self):
        self.generator = ColorMappingsHStyleGenerator()
        self.generator.generator_options = self.generator_options
        self.AddJSONFilesToModel(
            ['colors_ref_tokens_test.json5', 'colors_sys_tokens_test.json5'])
        self.assertEqualToFile(
            self.generator.Render(),
            'colors_tokens_test_color_mappings.h.generated')


class BlendStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'blend_colors_test.json5'])
        self.expected_output_file = 'blend_colors_test_expected.css'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)


class PreBlendStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'preblend_colors_test.json5'])
        self.expected_output_file = 'preblend_colors_test_expected.css'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)


class InvertedStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'inverted_colors_test.json5'])
        self.expected_output_file = 'inverted_colors_test_expected.css'

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)


if __name__ == '__main__':
    unittest.main()
