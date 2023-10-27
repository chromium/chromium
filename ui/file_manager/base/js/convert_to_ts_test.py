# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import unittest
from convert_to_ts import to_build_file, process_build_file, process_js_file, process_file_names_gni
from pprint import pprint
from pathlib import Path
from difflib import Differ

_HERE_DIR = os.path.dirname(__file__)


def to_expected(fname):
    name, ext = os.path.splitext(fname)
    return name + '_expected' + ext + '.txt'


class ConvertToTs(unittest.TestCase):
    def setUp(self):
        self.fname = Path(_HERE_DIR,
                          os.path.join('tests', 'testing_convert_to_ts.js'))
        self.unittest_fname = Path(
            _HERE_DIR,
            os.path.join('tests', 'testing_convert_to_ts_unittest.js'))
        self.build_file = to_build_file(self.fname).with_name('_BUILD.gn')
        self.file_names_gni = Path(_HERE_DIR,
                                   os.path.join('tests', '_file_names.gni'))

        # The file_names.gni we process different for JS and unittest.
        self.file_names_gni_js_result = Path(
            _HERE_DIR, os.path.join('tests',
                                    '_file_names_js_expected.gni.txt'))
        self.file_names_gni_unittest_result = Path(
            _HERE_DIR,
            os.path.join('tests', '_file_names_unittest_expected.gni.txt'))

    def compare_to_expected(self, content, fname, expected_fname=None):
        expected_fname = expected_fname or to_expected(fname)
        with open(expected_fname) as f:
            expected = f.readlines()
        try:
            self.assertEqual(content, expected)
        except BaseException:
            print('-- Conversion result ----------------------')
            for l in content:
                print(l.replace('\n', ''))
            print('-- Diff: ----------------------------------')
            for l in list(Differ().compare(expected, content)):
                print(l, end='')
            print('-------------------------------------------')

            raise

    def test_build_file(self):
        new_file = process_build_file(self.build_file, self.fname)
        self.compare_to_expected(new_file, self.build_file)

    def test_js_file(self):
        new_file = process_js_file(self.fname)
        self.compare_to_expected(new_file, self.fname)

    def test_file_names_js(self):
        new_file = process_file_names_gni(self.fname, self.file_names_gni)
        self.compare_to_expected(new_file, self.fname,
                                 self.file_names_gni_js_result)

    def test_file_names_unittest(self):
        new_file = process_file_names_gni(self.unittest_fname,
                                          self.file_names_gni)
        self.compare_to_expected(new_file, self.unittest_fname,
                                 self.file_names_gni_unittest_result)


if __name__ == '__main__':
    unittest.main()
