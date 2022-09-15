#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from pathlib import Path

if len(Path(__file__).parents) > 2:
    sys.path += [str(Path(__file__).parents[2])]

from style_variable_generator.find_invalid_css_variables import FindInvalidCSSVariables
import unittest


class FindInvalidCSSVariablesTest(unittest.TestCase):
    def testUnspecified(self):
        def GitResult(command):
            return b'''a:1:--test-not-specified
a:1:--test-only-rgb-used-rgb
a:1:--test-toolbar'''

        json_string = '''
{
  options: {
    CSS: {
      prefix: 'test'
    }
  },
  colors: {
    toolbar: "rgb(255, 255, 255)",
    only_rgb_used: "rgb(255, 255, 255)",
  }
}
        '''

        result = FindInvalidCSSVariables({'test': json_string},
                                         git_runner=GitResult)
        unused = set()
        self.assertEqual(result['unused'], unused)
        unspecified = ['a:1:--test-not-specified']
        self.assertEqual(result['unspecified'], unspecified)

    def testUnused(self):
        def GitResult(command):
            return b'''a:1:--test-toolbar'''

        json_string = '''
{
  options: {
    CSS: {
      prefix: 'test'
    }
  },
  colors: {
    toolbar: "rgb(255, 255, 255)",
    unused: "rgb(255, 255, 255)",
  },
  opacities: {
    unused_opacity: 0.3,
  },
  typography: {
    font_families: {
      font_family_unused: 'unused',
    },
    typefaces: {
      headline_1: {
        font_family: '$font_family_unused',
        font_size: 15,
        font_weight: 500,
        line_height: 22,
      },
    },
  },
  untyped_css: {
    custom_type: {
      unused_css: 'box-shadow',
    },
  },
}
        '''

        result = FindInvalidCSSVariables({'test': json_string},
                                         git_runner=GitResult)
        unused = set([
            'unused_opacity',
            'unused_css',
            'headline_1',
            'unused',
        ])
        self.assertEqual(result['unused'], unused)
        unspecified = []
        self.assertEqual(result['unspecified'], unspecified)

    def testNoPrefix(self):
        def GitResult(command):
            return ''

        json_string = '''
{
  colors: {
    toolbar: "rgb(255, 255, 255)",
  }
}
        '''
        self.assertRaises(KeyError,
                          FindInvalidCSSVariables, {'test': json_string},
                          git_runner=GitResult)


if __name__ == '__main__':
    unittest.main()
