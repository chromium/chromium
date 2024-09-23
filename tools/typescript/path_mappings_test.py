#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import path_mappings
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)
_CWD = os.getcwd()


class PathMappingsTest(unittest.TestCase):

  def setUp(self):
    self._out_folder = None
    self._additional_flags = []

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _build_path_map(self):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'test_mappings')
    path_mappings.main([
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--raw_deps',
        # Shouldn't map dependencies that aren't in the common mappings.
        '//chrome/browser/resources/print_preview:build_ts',
        '//ui/webui/resources/js:build_ts',
        '//ui/webui/resources/cr_components/localized_link:build_ts',
        '//third_party/polymer/v3_0:library',
        '//third_party/lit/v3_0:build_ts',
        '--output_suffix',
        'test_mappings',
        '--pretty_print',
    ] + self._additional_flags)
    return gen_dir

  def _assert_output(self, gen_dir, expected_file):
    output_file = 'path_mappings_test_mappings.json'
    self.assertTrue(os.path.exists(os.path.join(gen_dir, output_file)))
    expectations_dir = os.path.join(_HERE_DIR, 'tests', 'expected',
                                    'path_mappings')

    def _read_file(parent_dir, file_name):
      file_path = os.path.join(parent_dir, file_name)
      with open(file_path, 'r') as f:
        return f.read()

    actual = _read_file(gen_dir, output_file)
    expected = _read_file(expectations_dir, expected_file)
    self.assertMultiLineEqual(expected, actual)

  def testTrusted(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    gen_dir = self._build_path_map()
    self._assert_output(gen_dir, 'trusted_mappings_expected.json')

  def testUntrusted(self):
    self._additional_flags = ['--webui_context_type', 'untrusted']
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    gen_dir = self._build_path_map()
    self._assert_output(gen_dir, 'untrusted_mappings_expected.json')

  def testRelative(self):
    self._additional_flags = ['--webui_context_type', 'relative']
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    gen_dir = self._build_path_map()
    self._assert_output(gen_dir, 'relative_only_mappings_expected.json')

  def testTrustedOnly(self):
    self._additional_flags = ['--webui_context_type', 'trusted_only']
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    gen_dir = self._build_path_map()
    self._assert_output(gen_dir, 'trusted_only_mappings_expected.json')

if __name__ == '__main__':
  unittest.main()
