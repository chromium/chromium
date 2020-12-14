#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import modules
import os

_HERE_DIR = os.path.dirname(__file__)


class ModulesConversionTest(unittest.TestCase):
    def _get_file_lines(self, file):
        path = os.path.join(_HERE_DIR, 'tests', file)
        return modules.get_file_lines(path)

    def _debug_file_contents(self, file_lines, expected_file_lines):
        print '\n------- FILE -------'
        print '\n'.join(file_lines)
        print '----- EXPECTED -----'
        print '\n'.join(expected_file_lines)
        print '--------------------\n'

    def testExportFunction(self):
        '''Tests exporting functions that are not used locally.'''
        file_lines = self._get_file_lines('export_function.js')
        expected_file_lines = self._get_file_lines(
            'export_function_expected.js')

        # Check: function is exported correctly.
        modules.add_js_file_exports(file_lines)
        self.assertEquals(file_lines, expected_file_lines)

        # Check: running add_js_file_exports a second time produces the same
        # expected output.
        modules.add_js_file_exports(file_lines)
        self.assertEquals(file_lines, expected_file_lines)

    def testExportLocalFunction(self):
        '''Tests that functions that are used locally are not be exported.'''
        file_lines = self._get_file_lines('export_local_function.js')
        expected_file_lines = self._get_file_lines(
            'export_local_function_expected.js')

        # Check: local function is not exported.
        modules.add_js_file_exports(file_lines)
        self.assertEquals(file_lines, expected_file_lines)

    def testUpdateEmptyDependencyList(self):
        '''Tests adding a dependency to an empty dependency list.'''
        file_lines = self._get_file_lines('empty_dependency_list.gn')
        expected_file_lines = self._get_file_lines(
            'empty_dependency_list_expected.gn')
        rule_first_line = 'js_unittest("importer_common_unittest.m") {'
        list_name = 'deps'
        dependency_line = '    "//ui/file_manager/base/js:mock_chrome",'

        # Check: dependency list correctly updated with new dependency.
        modules.add_dependency(file_lines, rule_first_line, list_name,
                               dependency_line)
        self.assertEquals(file_lines, expected_file_lines)

        # Check: running add_dependency a second time produces the same
        # expected output (no duplicate dependency).
        modules.add_dependency(file_lines, rule_first_line, list_name,
                               dependency_line)
        self.assertEquals(file_lines, expected_file_lines)

    def testUpdateSingleLineDependencyList(self):
        '''Tests adding a dependency to an single-line dependency list.'''
        file_lines = self._get_file_lines('single_line_dependency_list.gn')
        expected_file_lines = self._get_file_lines(
            'single_line_dependency_list_expected.gn')
        rule_first_line = 'js_unittest("importer_common_unittest.m") {'
        list_name = 'deps'
        dependency_line = '    "//ui/file_manager/base/js:mock_chrome",'

        # Check: dependency list correctly formatted and updated with new
        # dependency.
        modules.add_dependency(file_lines, rule_first_line, list_name,
                               dependency_line)
        self.assertEquals(file_lines, expected_file_lines)


if __name__ == '__main__':
    unittest.main()
