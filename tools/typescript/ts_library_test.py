#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import ts_library
import ts_definitions
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)


class TsLibraryTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._additional_flags = []

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _build_project1(self):
    gen_dir = os.path.join(self._out_folder, 'project1')

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
    ts_library.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project1'),
        '--gen_dir',
        gen_dir,
        '--out_dir',
        gen_dir,
        '--in_files',
        'foo.ts',
        '--definitions',
        'legacy_file.d.ts',
        '--composite',
    ])
    return gen_dir

  def _assert_project1_output(self, gen_dir):
    files = [
        'foo.d.ts',
        'foo.js',
        'legacy_file.d.ts',
        'tsconfig_definitions.json',
        'tsconfig.json',
        'tsconfig.manifest',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    # Check that the generated .tsbuildinfo file is deleted.
    tsbuildinfo = 'tsconfig.tsbuildinfo'
    self.assertFalse(os.path.exists(os.path.join(gen_dir, tsbuildinfo)),
                     tsbuildinfo)

  # Builds project2 which depends on files from project1 and project3, both via
  # relative URLs, as well as via absolute chrome:// URLs.
  def _build_project2(self, project1_gen_dir, project3_gen_dir):
    root_dir = os.path.join(_HERE_DIR, 'tests', 'project2')
    gen_dir = os.path.join(self._out_folder, 'project2')
    project1_gen_dir = os.path.relpath(project1_gen_dir, gen_dir)
    project3_gen_dir = os.path.relpath(project3_gen_dir, gen_dir)

    ts_library.main([
        '--root_dir',
        root_dir,
        '--gen_dir',
        gen_dir,
        '--out_dir',
        gen_dir,
        '--in_files',
        'bar.ts',
        '--deps',
        os.path.join(project1_gen_dir, 'tsconfig.json'),
        os.path.join(project3_gen_dir, 'tsconfig.json'),
        '--path_mappings',
        'chrome://some-other-source/*|' + os.path.join(project1_gen_dir, '*'),
        '--tsconfig_base',
        os.path.relpath(os.path.join(root_dir, 'tsconfig_base.json'), gen_dir),
    ])
    return gen_dir

  def _assert_project2_output(self, gen_dir):
    files = [
        'bar.js',
        'tsconfig.json',
        'tsconfig.manifest',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    non_existing_files = [
        'bar.d.ts',
        'tsconfig.tsbuildinfo',
    ]
    for f in non_existing_files:
      self.assertFalse(os.path.exists(os.path.join(gen_dir, f)), f)

  # Builds project3, which includes only definition files.
  def _build_project3(self):
    gen_dir = os.path.join(self._out_folder, 'project3')

    ts_library.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project3'),
        '--gen_dir',
        gen_dir,
        '--out_dir',
        gen_dir,
        '--definitions',
        '../../tests/project3/baz.d.ts',
        '--composite',
    ])
    return gen_dir

  def _assert_project3_output(self, gen_dir):
    self.assertTrue(os.path.exists(os.path.join(gen_dir, 'tsconfig.json')))
    self.assertFalse(
        os.path.exists(os.path.join(gen_dir, 'tsconfig.tsbuildinfo')))
    self.assertFalse(os.path.exists(os.path.join(gen_dir, 'tsconfig.manifest')))

  def _build_project4(self):
    gen_dir = os.path.join(self._out_folder, 'project4')

    # Build project4, which includes multiple TS files, only one of which should
    # be included in the manifest.
    ts_library.main([
        '--root_dir',
        os.path.join(_HERE_DIR, 'tests', 'project4'),
        '--gen_dir',
        gen_dir,
        '--out_dir',
        gen_dir,
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
        'tsconfig.json',
        'tsconfig.manifest',
    ]
    for f in files:
      self.assertTrue(os.path.exists(os.path.join(gen_dir, f)), f)

    # Check that the generated manifest file doesn't include exclude.js.
    manifest = 'tsconfig.manifest'
    with open(os.path.join(gen_dir, manifest), 'r') as f:
      data = json.load(f)
      self.assertEqual(len(data['files']), 1)
      self.assertEqual(data['files'][0], 'include.js')


  # Test success case where both project1 and project2 are compiled successfully
  # and no errors are thrown.
  def testSuccess(self):
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    project1_gen_dir = self._build_project1()
    self._assert_project1_output(project1_gen_dir)

    project3_gen_dir = self._build_project3()
    self._assert_project3_output(project3_gen_dir)

    project2_gen_dir = self._build_project2(project1_gen_dir, project3_gen_dir)
    self._assert_project2_output(project2_gen_dir)

    project4_gen_dir = self._build_project4()
    self._assert_project4_output(project4_gen_dir)

  # Test error case where a type violation exists, ensure that an error is
  # thrown.
  def testError(self):
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    gen_dir = os.path.join(self._out_folder, 'project1')
    try:
      ts_library.main([
          '--root_dir',
          os.path.join(_HERE_DIR, 'tests', 'project1'),
          '--gen_dir',
          gen_dir,
          '--out_dir',
          gen_dir,
          '--in_files',
          'errors.ts',
          '--composite',
      ])
    except RuntimeError as err:
      self.assertTrue('Type \'number\' is not assignable to type \'string\'' \
                      in str(err))
      self.assertFalse(
          os.path.exists(os.path.join(gen_dir, 'tsconfig.tsbuildinfo')))
    else:
      self.fail('Failed to detect type error')


if __name__ == '__main__':
  unittest.main()
