#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

sys.path += [os.path.dirname(os.path.dirname(__file__))]

from style_variable_generator.find_invalid_css_variables import FindInvalidCSSVariables
import unittest


class FindInvalidCSSVariablesTest(unittest.TestCase):
    def testUnspecified(self):
        def GitResult(command):
            return b'''--test-not-specified
--test-only-rgb-used-rgb
--test-toolbar'''

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
        unspecified = set(['--test-not-specified'])
        self.assertEqual(result['unspecified'], unspecified)

    def testUnused(self):
        def GitResult(command):
            return b'''--test-toolbar'''

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
  }
}
        '''

        result = FindInvalidCSSVariables({'test': json_string},
                                         git_runner=GitResult)
        unused = set(['--test-unused'])
        self.assertEqual(result['unused'], unused)
        unspecified = set()
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
                          FindInvalidCSSVariables,
                          {'test': json_string},
                          git_runner=GitResult)


if __name__ == '__main__':
    unittest.main()
