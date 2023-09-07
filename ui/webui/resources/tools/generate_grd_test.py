#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_grd
import os
import shutil
import sys
import tempfile
import unittest


_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)
pathToHere = os.path.relpath(_HERE_DIR, _CWD)

# This needs to be a constant, because if this expected file is checked into
# the repo, translation.py will try to find the dummy grdp files, which don't
# exist. We can't alter the translation script, so work around it by
# hardcoding this string, instead of checking in even more dummy files.
EXPECTED_GRD_WITH_GRDP_FILES = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="0" current_release="1" output_all_resource_defines="false">
  <outputs>
    <output filename="grit/test_resources.h" type="rc_header">
      <emit emit_type='prepend'></emit>
    </output>
    <output filename="grit/test_resources_map.cc"
            type="resource_file_map_source" />
    <output filename="grit/test_resources_map.h"
            type="resource_map_header" />
    <output filename="test_resources.pak" type="data_package" />
  </outputs>
  <release seq="1">
    <includes>
      <part file="foo_resources.grdp" />
      <part file="foo/bar_resources.grdp" />
    </includes>
  </release>
</grit>\n'''

class GenerateGrdTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    file_path = os.path.join(self._out_folder, file_name)
    with open(file_path, 'r', newline='', encoding='utf-8') as f:
      return f.read()

  def _run_test_(self, grd_expected,
                 out_grd='test_resources.grd',
                 manifest_files=None, input_files=None,
                 input_files_base_dir=None, output_files_base_dir=None,
                 grdp_files=None, resource_path_rewrites=None,
                 resource_path_prefix=None):
    args = [
      '--out-grd', os.path.join(self._out_folder, out_grd),
      '--grd-prefix', 'test',
      '--root-gen-dir', os.path.join(_CWD, pathToHere, 'tests'),
    ]

    if manifest_files != None:
      args += [
        '--manifest-files',
      ] + manifest_files

    if grdp_files != None:
      args += [
        '--grdp-files',
      ] + grdp_files

    if (input_files_base_dir):
      args += [
        '--input-files-base-dir',
        input_files_base_dir,
        '--input-files',
      ] + input_files

    if (output_files_base_dir):
      args += [
        '--output-files-base-dir',
        output_files_base_dir,
      ]

    if (resource_path_rewrites):
      args += [ '--resource-path-rewrites' ] + resource_path_rewrites

    if (resource_path_prefix):
      args += [ '--resource-path-prefix', resource_path_prefix ]

    generate_grd.main(args)

    actual_grd = self._read_out_file(out_grd)
    if (grd_expected.endswith('.grd') or grd_expected.endswith('.grdp')):
      expected_grd_path = os.path.join(_HERE_DIR, 'tests', 'generate_grd',
                                       grd_expected)
      with open(expected_grd_path, 'r', newline='', encoding='utf-8') as f:
        expected_grd_content = f.read()
      self.assertMultiLineEqual(expected_grd_content, actual_grd)
    else:
      self.assertMultiLineEqual(grd_expected, actual_grd)

  def testSuccess(self):
    self._run_test_(
        'expected_grd.grd',
        manifest_files=[
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_1.json'),
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_2.json'),
        ])

  def testSuccessWithInputFiles(self):
    self._run_test_(
        'expected_grd_with_input_files.grd',
        manifest_files=[
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_1.json'),
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_2.json'),
        ],
        input_files=['images/test_svg.svg', 'test_html_in_src.html'],
        input_files_base_dir='test_src_dir')

  def testSuccessWithGeneratedInputFiles(self):
    # For generated |input_files|, |input_files_base_dir| must be a
    # sub-directory of |root_gen_dir|.
    base_dir = os.path.join(_CWD, pathToHere, 'tests', 'foo', 'bar')
    self._run_test_(
      'expected_grd_with_generated_input_files.grd',
      input_files = [ 'baz/a.svg', 'b.svg' ],
      input_files_base_dir = base_dir)

  def testSuccessWithGrdpFiles(self):
    self._run_test_(
      EXPECTED_GRD_WITH_GRDP_FILES,
      grdp_files = [
        os.path.join(self._out_folder, 'foo_resources.grdp'),
        os.path.join(self._out_folder, 'foo', 'bar_resources.grdp'),
      ])

  def testSuccessGrdpWithInputFiles(self):
    self._run_test_(
      'expected_grdp_with_input_files.grdp',
      out_grd = 'test_resources.grdp',
      input_files = [ 'images/test_svg.svg', 'test_html_in_src.html' ],
      input_files_base_dir = 'test_src_dir')

  def testSuccessGrdpWithResourcePathPrefix(self):
    self._run_test_(
      'expected_grdp_with_resource_path_prefix.grdp',
      out_grd = 'test_resources.grdp',
      input_files = [ 'foo.js', 'bar.svg' ],
      input_files_base_dir = 'test_src_dir',
      resource_path_prefix = 'baz')

  def testSuccessWithRewrites(self):
    self._run_test_(
        'expected_grd_with_rewrites.grd',
        manifest_files=[
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_1.json'),
            os.path.join(pathToHere, 'tests', 'generate_grd',
                         'test_manifest_2.json'),
        ],
        resource_path_rewrites=[
            'test.rollup.js|test.js',
            'dir/another_element_in_dir.js|dir2/another_element_in_dir_renamed.js',
        ])

  def testSuccessWithOutputFilesBaseDir(self):
    self._run_test_(
      'expected_grd_with_output_files_base_dir.grd',
      input_files = [ 'images/test_svg.svg' ],
      input_files_base_dir = 'test_src_dir',
      output_files_base_dir = 'foo/bar')


if __name__ == '__main__':
  unittest.main()
