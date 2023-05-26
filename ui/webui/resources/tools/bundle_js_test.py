#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import bundle_js
import os
import shutil
import tempfile
import unittest

_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)


class BundleJsTest(unittest.TestCase):

  def setUp(self):
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name), 'r') as f:
      return f.read()

  def _check_dep_file(self, paths_from_test_dir, depfile_content):
    for path in paths_from_test_dir:
      abs_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', path)
      rel_path = os.path.relpath(abs_path, _CWD)
      self.assertIn(os.path.normpath(rel_path), depfile_content)

  def _run_bundle(self, input_args):
    input_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'src')
    resources_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'resources')
    custom_dir_foo = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'external',
                                  'foo')
    custom_dir_bar = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'external',
                                  'bar')
    args = input_args + [
        '--depfile',
        os.path.join(self._out_folder, 'depfile.d'),
        '--target_name',
        'dummy_target_name',
        '--input',
        input_path,
        '--out_folder',
        self._out_folder,
        '--external_paths',
        '//resources|%s' % resources_path,
        'chrome://resources|%s' % resources_path,
        'chrome-untrusted://resources|%s' % resources_path,
        'some-fake-scheme://foo|%s' % os.path.abspath(custom_dir_foo),
        'some-fake-scheme://bar|%s' % os.path.abspath(custom_dir_bar),
    ]
    bundle_js.main(args)

  def testSimpleBundle(self):
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'foo_ui.js',
    ]
    self._run_bundle(args)

    output_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', output_js)
    self.assertIn('Hello from src/subdir/baz.js', output_js)

    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/foo.js', 'src/subdir/baz.js'], depfile_d)

  def testBundleWithResources(self):
    resources_path = './tests/bundle_js/resources'
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'foo_ui.js',
    ]
    self._run_bundle(args)

    ui_rollup_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from resources/foo_resource.js', ui_rollup_js)
    self.assertIn('Hello from resources/bar_resource.js', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(
        ['resources/foo_resource.js', 'resources/bar_resource.js'], depfile_d)

  def testMultiBundleBundle(self):
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'foo_ui.js',
        'bar_ui.js',
        '--out-manifest',
        os.path.join(self._out_folder, 'out_manifest.json'),
    ]
    self._run_bundle(args)

    # Check that the shared element is in the shared bundle and the non-shared
    # elements are in the individual bundles.
    foo_ui_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', foo_ui_js)
    self.assertNotIn('Hello from src/bar.js', foo_ui_js)
    self.assertNotIn('Hello from src/subdir/baz.js', foo_ui_js)

    bar_ui_js = self._read_out_file('bar_ui.rollup.js')
    self.assertNotIn('Hello from src/foo.js', bar_ui_js)
    self.assertIn('Hello from src/bar.js', bar_ui_js)
    self.assertNotIn('Hello from src/subdir/baz.js', bar_ui_js)

    shared_js = self._read_out_file('shared.rollup.js')
    self.assertNotIn('Hello from src/foo.js', shared_js)
    self.assertNotIn('Hello from src/bar.js', shared_js)
    self.assertIn('Hello from src/subdir/baz.js', shared_js)

    # All 3 JS files should be in the depfile.
    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/foo.js', 'src/bar.js', 'src/subdir/baz.js'],
                         depfile_d)

    manifest = json.loads(self._read_out_file('out_manifest.json'))
    self.assertEqual(3, len(manifest['files']))
    self.assertTrue('foo_ui.rollup.js' in manifest['files'])
    self.assertTrue('bar_ui.rollup.js' in manifest['files'])
    self.assertTrue('shared.rollup.js' in manifest['files'])

    self.assertEqual(
        os.path.relpath(self._out_folder, _CWD).replace('\\', '/'),
        os.path.relpath(manifest['base_dir'], _CWD))

  def testSimpleBundleExcludes(self):
    args = [
        '--host',
        'chrome-extension://myextensionid/',
        '--js_module_in_files',
        'foo_ui.js',
        '--exclude',
        'foo.js',
    ]
    self._run_bundle(args)

    output_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/subdir/baz.js', output_js)
    self.assertNotIn('Hello from src/foo.js', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/subdir/baz.js'], depfile_d)
    self.assertNotIn('src/foo.js', depfile_d)

  # Tests that bundling resources for an untrusted UI can successfully exclude
  # resources imported from both chrome-untrusted://resources and scheme
  # relative paths.
  def testSimpleBundleExcludesResources(self):
    args = [
        '--host',
        'chrome-untrusted://fake-host',
        '--js_module_in_files',
        'foo_ui.js',
        '--exclude',
        '//resources/bar_resource.js',
        'chrome-untrusted://resources/foo_untrusted.js',
    ]
    self._run_bundle(args)

    output_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', output_js)
    self.assertIn('chrome-untrusted://resources/foo_untrusted.js', output_js)
    self.assertIn('//resources/bar_resource.js', output_js)
    self.assertNotIn('Hello from resources/foo_untrusted.js', output_js)
    self.assertNotIn('Hello from resources/bar_resource.js', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self.assertNotIn('resources/foo_untrusted.js', depfile_d)
    self.assertNotIn('resources/bar_resource.js', depfile_d)

  # Tests that bundling resources for an untrusted UI successfully bundles
  # resources from both chrome-untrusted://resources and //resources.
  def testSimpleBundleUntrustedResources(self):
    args = [
        '--host',
        'chrome-untrusted://fake-host',
        '--js_module_in_files',
        'foo_ui.js',
    ]
    self._run_bundle(args)

    output_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', output_js)
    self.assertIn('Hello from resources/foo_untrusted.js', output_js)
    self.assertIn('Hello from resources/bar_resource.js', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(
        ['resources/foo_untrusted.js', 'resources/bar_resource.js'], depfile_d)

  def testBundleWithCustomLayeredPaths(self):
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'foo_ui.js',
    ]
    self._run_bundle(args)

    ui_rollup_js = self._read_out_file('foo_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', ui_rollup_js)
    self.assertIn('Hello from external/foo/foo.js', ui_rollup_js)
    self.assertIn('Hello from external/bar/bar.js', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(
        ['src/foo.js', 'external/foo/foo.js', 'external/bar/bar.js'], depfile_d)

  def testBundleWithBundleSubpath(self):
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'subdir/baz_ui.js',
    ]
    self._run_bundle(args)

    ui_rollup_js = self._read_out_file('subdir/baz_ui.rollup.js')
    self.assertIn('Hello from src/foo.js', ui_rollup_js)
    self.assertIn('Hello from src/subdir/baz.js', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/foo.js', 'src/subdir/baz.js'], depfile_d)


if __name__ == '__main__':
  unittest.main()
