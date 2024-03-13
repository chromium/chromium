#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import ts_library
import ts_definitions
import path_mappings
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)
_CWD = os.getcwd()


class TsLibraryTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._additional_flags = []

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _build_project1(self, enable_source_maps=False):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project1')

    # Generate definition .d.ts file for legacy JS file.
    ts_definitions.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project1'),
        '--gen_dir',
        gen_dir,
        '--out_dir',
        gen_dir,
        '--js_files',
        'legacy_file.js',
    ])

    # Build project1, which includes a mix of TS and definition files.
    args = [
        '--output_suffix',
        'build_ts',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project1'), _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--in_files',
        'foo.ts',
        '--definitions',
        'legacy_file.d.ts',
        '--composite',
    ]

    if enable_source_maps:
      args += ['--enable_source_maps']

    ts_library.main(args)
    return gen_dir

  def _assert_project1_output(self, gen_dir):
    files = [
        'foo.d.ts',
        'foo.js',
        'legacy_file.d.ts',
        'tsconfig_definitions.json',
        'tsconfig_build_ts.json',
        'build_ts_manifest.json',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    # Check that the generated .tsbuildinfo file is deleted.
    tsbuildinfo = 'tsconfig_build_ts.tsbuildinfo'
    self.assertFalse(os.path.exists(os.path.join(gen_dir, tsbuildinfo)),
                     tsbuildinfo)

  # Builds project2 which depends on files from project1 and project3, project6,
  # both via relative URLs, as well as via absolute chrome:// and
  # chrome://resources/ URLs.
  def _build_project2(self, project1_gen_dir, project3_gen_dir,
                      project6_gen_dir):
    root_dir = os.path.join(_HERE_DIR, 'tests', 'project2')
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project2')
    project1_gen_dir = os.path.relpath(project1_gen_dir, gen_dir)
    project3_gen_dir = os.path.relpath(project3_gen_dir, gen_dir)
    project6_gen_dir = os.path.relpath(project6_gen_dir, gen_dir)
    # Using path mappings to generate the path map file. path_mappings is also
    # unit tested separately in path_mappings_test.py.
    path_mappings.main([
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--raw_deps',
        '//ui/webui/resources/js:build_ts',
        '--output_suffix',
        'project2',
    ])

    ts_library.main([
        '--output_suffix',
        'build_ts',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(root_dir, _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--in_files',
        'bar.ts',
        '--deps',
        os.path.join(project1_gen_dir, 'tsconfig_build_ts.json'),
        os.path.join(project3_gen_dir, 'tsconfig_build_ts.json'),
        os.path.join(project6_gen_dir, 'tsconfig_build_ts.json'),
        '--path_mappings',
        'chrome://some-other-source/*|' + os.path.join(project1_gen_dir, '*'),
        '--path_mappings_file',
        'path_mappings_project2.json',
        '--tsconfig_base',
        os.path.relpath(os.path.join(root_dir, 'tsconfig_base.json'), gen_dir),
    ])
    return gen_dir

  def _assert_project2_output(self, gen_dir):
    files = [
        'bar.js',
        'tsconfig_build_ts.json',
        'build_ts_manifest.json',
        'path_mappings_project2.json',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    non_existing_files = [
        'bar.d.ts',
        'tsconfig_build_ts.tsbuildinfo',
    ]
    for f in non_existing_files:
      self.assertFalse(os.path.exists(os.path.join(gen_dir, f)), f)

  # Builds project3, which includes only definition files.
  def _build_project3(self):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project3')

    ts_library.main([
        '--output_suffix',
        'build_ts',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project3'), _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--definitions',
        os.path.relpath(
            os.path.join(_HERE_DIR, 'tests', 'project3', 'baz.d.ts'), gen_dir),
        '--composite',
    ])
    return gen_dir

  def _assert_project3_output(self, gen_dir):
    self.assertTrue(
        os.path.exists(os.path.join(gen_dir, 'tsconfig_build_ts.json')))
    self.assertFalse(
        os.path.exists(os.path.join(gen_dir, 'tsconfig_build_ts.tsbuildinfo')))
    self.assertFalse(
        os.path.exists(os.path.join(gen_dir, 'build_ts_manifest.json')))

  def _build_project4(self):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project4')

    # Build project4, which includes multiple TS files, only one of which should
    # be included in the manifest.
    ts_library.main([
        '--output_suffix',
        'build_ts',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project4'), _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--in_files',
        'include.ts',
        'exclude.ts',
        '--manifest_excludes',
        'exclude.ts',
    ])
    return gen_dir

  def _assert_project4_output(self, gen_dir):
    files = [
        'include.js',
        'exclude.js',
        'tsconfig_build_ts.json',
        'build_ts_manifest.json',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    # Check that the generated manifest file doesn't include exclude.js.
    manifest = os.path.join(gen_dir, 'build_ts_manifest.json')
    self._assert_manifest_files(manifest, ['include.js'])

  def _assert_manifest_files(self, manifest_path, expected_files):
    with open(manifest_path, 'r') as f:
      data = json.load(f)
      self.assertEqual(data['files'], expected_files)

  def _build_project5(self):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project5')
    out_dir_test = os.path.join(self._out_folder, 'project5_test')

    # Build project5, which includes 2 TS projects one for prod and one for
    # test, it should generate different manifest, tsconfig and tsbuildinfo.
    # prod:
    ts_library.main([
        '--output_suffix',
        'build_ts',
        '--composite',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project5'), _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--in_files',
        'bar.ts',
    ])

    # test:
    ts_library.main([
        '--output_suffix',
        'test_build_ts',
        '--deps',
        os.path.join(gen_dir, 'tsconfig_build_ts.json'),
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project5'), _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(gen_dir, _CWD),
        '--in_files',
        'bar_test.ts',
    ])

    return gen_dir

  def _assert_project5_output(self, gen_dir):
    # prod:
    self.assertTrue(
        os.path.exists(os.path.join(gen_dir, 'tsconfig_build_ts.json')))
    manifest = os.path.join(gen_dir, 'build_ts_manifest.json')
    self.assertTrue(os.path.exists(manifest))
    self._assert_manifest_files(manifest, ['bar.js'])

    # test:
    self.assertTrue(
        os.path.exists(os.path.join(gen_dir, 'tsconfig_test_build_ts.json')))
    manifest_test = os.path.join(gen_dir, 'test_build_ts_manifest.json')
    self.assertTrue(os.path.exists(manifest_test))
    self._assert_manifest_files(manifest_test, ['bar_test.js'])

  def _build_project6(self):
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'ui', 'webui', 'resources', 'js')
    out_dir = os.path.join(self._out_folder, 'ui', 'webui', 'resources', 'tsc',
                           'js')

    # Build project6, which simulates the build setup and location of shared
    # ui/webui/resources/ projects, and is used to test the codepath that infers
    # |path_mappings| from |raw_deps|.
    ts_library.main([
        '--output_suffix',
        'build_ts',
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--root_dir',
        os.path.relpath(
            os.path.join(_HERE_DIR, 'tests', 'ui', 'webui', 'resources', 'js'),
            _CWD),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--out_dir',
        os.path.relpath(out_dir, _CWD),
        '--in_files',
        'assert.ts',
        '--composite',
    ])

    return (gen_dir, out_dir)

  def _assert_project6_output(self, gen_dir, out_dir):
    gen_dir_files = [
        'tsconfig_build_ts.json',
        'build_ts_manifest.json',
    ]
    for f in gen_dir_files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    # Check that the generated .tsbuildinfo file is deleted.
    tsbuildinfo = 'tsconfig_build_ts.tsbuildinfo'
    self.assertFalse(os.path.exists(os.path.join(gen_dir, tsbuildinfo)),
                     tsbuildinfo)

    out_dir_files = [
        'assert.d.ts',
        'assert.js',
    ]
    for f in out_dir_files:
      self.assertTrue(os.path.exists(os.path.join(out_dir, f)), f)


  # Test success case where both project1 and project2 are compiled successfully
  # and no errors are thrown.
  def testSuccess(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    project1_gen_dir = self._build_project1()
    self._assert_project1_output(project1_gen_dir)

    project3_gen_dir = self._build_project3()
    self._assert_project3_output(project3_gen_dir)

    (project6_gen_dir, project6_out_dir) = self._build_project6()
    self._assert_project6_output(project6_gen_dir, project6_out_dir)

    project2_gen_dir = self._build_project2(project1_gen_dir, project3_gen_dir,
                                            project6_gen_dir)
    self._assert_project2_output(project2_gen_dir)

    project4_gen_dir = self._build_project4()
    self._assert_project4_output(project4_gen_dir)

    project5_gen_dir = self._build_project5()
    self._assert_project5_output(project5_gen_dir)

  # Test error case where a type violation exists, ensure that an error is
  # thrown.
  def testError(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project1')
    try:
      ts_library.main([
          '--output_suffix',
          'build_ts',
          '--root_gen_dir',
          os.path.relpath(self._out_folder, gen_dir),
          '--root_src_dir',
          os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
          '--root_dir',
          os.path.relpath(os.path.join(_HERE_DIR, 'tests', 'project1'), _CWD),
          '--gen_dir',
          os.path.relpath(gen_dir, _CWD),
          '--out_dir',
          os.path.relpath(gen_dir, _CWD),
          '--in_files',
          'errors.ts',
          '--composite',
      ])
    except RuntimeError as err:
      self.assertTrue('Type \'number\' is not assignable to type \'string\'' \
                      in str(err))
      self.assertFalse(
          os.path.exists(os.path.join(gen_dir,
                                      'tsconfig_build_ts.tsbuildinfo')))
    else:
      self.fail('Failed to detect type error')

  # Test error case where the project's tsconfig file is failing validation.
  def testTsConfigValidationError(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    root_dir = os.path.join(_HERE_DIR, 'tests', 'project5')
    gen_dir = os.path.join(self._out_folder, 'tools', 'typescript', 'tests',
                           'project5')
    try:
      ts_library.main([
          '--output_suffix',
          'build_ts',
          '--root_gen_dir',
          os.path.relpath(self._out_folder, gen_dir),
          '--root_src_dir',
          os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
          '--root_dir',
          os.path.relpath(root_dir, _CWD),
          '--gen_dir',
          os.path.relpath(gen_dir, _CWD),
          '--out_dir',
          os.path.relpath(gen_dir, _CWD),
          '--in_files',
          'bar.ts',
          '--tsconfig_base',
          os.path.relpath(os.path.join(root_dir, 'tsconfig_base.json'),
                          gen_dir),
      ])
    except AssertionError as err:
      self.assertTrue(
          str(err).startswith('Invalid |composite| flag detected in '))
    else:
      self.fail('Failed to detect error')

  # Test case where |enable_source_maps| is specified.
  def testEnableSourceMaps(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)

    expectations_dir = os.path.join(_HERE_DIR, 'tests', 'expected', 'project1')

    # Build project1 which source maps enabled.
    gen_dir = self._build_project1(enable_source_maps=True)

    # Assert output is as expected.
    def _read_file(parent_dir, file_name):
      file_path = os.path.join(gen_dir, file_name)
      with open(file_path, 'r', newline='') as f:
        return f.read()

    actual_js = _read_file(gen_dir, 'foo.js')
    expected_js = _read_file(expectations_dir, 'foo.js')
    self.assertMultiLineEqual(expected_js, actual_js)


if __name__ == '__main__':
  unittest.main()
