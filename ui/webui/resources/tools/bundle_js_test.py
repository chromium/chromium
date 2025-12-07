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
    self.maxDiff = None

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_file(self, file):
    with open(file, 'r') as f:
      return f.read()

  def _read_out_file(self, file_name):
    assert self._out_folder
    return self._read_file(os.path.join(self._out_folder, file_name))

  def _check_dep_file(self, paths_from_test_dir, depfile_content):
    for path in paths_from_test_dir:
      abs_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', path)
      rel_path = os.path.relpath(abs_path, _CWD)
      self.assertIn(os.path.normpath(rel_path), depfile_content)

  def _check_bundle_output(self, expected_bundle_name, actual_bundle_name):
    expected_js = self._read_file(
        os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'expected',
                     expected_bundle_name))
    actual_js = self._read_file(
        os.path.join(self._out_folder, actual_bundle_name))
    self.assertMultiLineEqual(expected_js, actual_js)

  def _check_dep_file(self, paths_from_test_dir, depfile_content):
    for path in paths_from_test_dir:
      abs_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', path)
      rel_path = os.path.relpath(abs_path, _CWD)
      self.assertIn(os.path.normpath(rel_path), depfile_content)

  def _get_external_paths_args(self):
    resources_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'resources')
    custom_dir_foo = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'external',
                                  'foo')
    custom_dir_baz = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'external',
                                  'baz')
    custom_dir_bar = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'external',
                                  'bar')
    return [
        '--external_paths',
        '//resources|%s' % resources_path,
        'chrome://resources|%s' % resources_path,
        'chrome-untrusted://resources|%s' % resources_path,
        # Test case where an exact URL is redirected, not just a prefix.
        'some-fake-scheme://foo/baz.js|%s' %
        os.path.abspath(os.path.join(custom_dir_baz, 'baz.js')),
        'some-fake-scheme://foo|%s' % os.path.abspath(custom_dir_foo),
        'some-fake-scheme://bar|%s' % os.path.abspath(custom_dir_bar),
    ]

  def _run_bundle(self, custom_args):
    input_path = os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'src')

    args = [
        '--depfile',
        os.path.join(self._out_folder, 'depfile.d'),
        '--target_name',
        'dummy_target_name',
        '--input',
        input_path,
        '--out_folder',
        self._out_folder,
    ] + self._get_external_paths_args() + custom_args
    bundle_js.main(args)

  def testSimpleBundle(self):
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'foo_ui.js',
    ]
    self._run_bundle(args)

    self._check_bundle_output('foo_ui.rollup.js', 'foo_ui.rollup.js')
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

    self._check_bundle_output('foo_ui.rollup.js', 'foo_ui.rollup.js')
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
    self._check_bundle_output('foo_ui_multi_bundle.rollup.js',
                              'foo_ui.rollup.js')
    self._check_bundle_output('bar_ui.rollup.js', 'bar_ui.rollup.js')
    self._check_bundle_output('shared.rollup.js', 'shared.rollup.js')

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
        os.path.normpath(os.path.relpath(self._out_folder, _CWD)),
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

    self._check_bundle_output('foo_ui_excludes.rollup.js', 'foo_ui.rollup.js')
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
    self._check_bundle_output('foo_ui_excludes_resources.rollup.js',
                              'foo_ui.rollup.js')
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

    self._check_bundle_output('foo_ui.rollup.js', 'foo_ui.rollup.js')
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

    self._check_bundle_output('foo_ui.rollup.js', 'foo_ui.rollup.js')
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

    self._check_bundle_output('baz_ui.rollup.js', 'subdir/baz_ui.rollup.js')
    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/foo.js', 'src/subdir/baz.js'], depfile_d)

  def testSimpleBundleWithCustomConfig(self):
    args = [
        '--host', 'fake-host', '--js_module_in_files', 'bar_wrapper.js',
        '--rollup_config',
        os.path.join(_HERE_DIR, 'tests', 'bundle_js', 'dummy.config.mjs')
    ]
    self._run_bundle(args)

    self._check_bundle_output('bar_wrapper.rollup.js', 'bar_wrapper.rollup.js')
    depfile_d = self._read_out_file('depfile.d')
    self._check_dep_file(['src/bar.js'], depfile_d)


if __name__ == '__main__':
  unittest.main()
