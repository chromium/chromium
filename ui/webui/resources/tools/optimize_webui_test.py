#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optimize_webui
import os
import shutil
import tempfile
import unittest

_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)


class OptimizeWebUiTest(unittest.TestCase):

  def setUp(self):
    self._tmp_dirs = []
    self._tmp_src_dir = None
    self._out_folder = self._create_tmp_dir()

  def tearDown(self):
    for tmp_dir in self._tmp_dirs:
      shutil.rmtree(tmp_dir)

  def _write_file_to_dir(self, file_path, file_contents):
    file_dir = os.path.dirname(file_path)
    if not os.path.exists(file_dir):
      os.makedirs(file_dir)
    with open(file_path, 'w') as tmp_file:
      tmp_file.write(file_contents)

  def _write_file_to_src_dir(self, file_path, file_contents):
    if not self._tmp_src_dir:
      self._tmp_src_dir = self._create_tmp_dir()
    file_path_normalized = os.path.normpath(
        os.path.join(self._tmp_src_dir, file_path))
    self._write_file_to_dir(file_path_normalized, file_contents)

  def _create_tmp_dir(self):
    # TODO(dbeam): support cross-drive paths (i.e. d:\ vs c:\).
    tmp_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self._tmp_dirs.append(tmp_dir)
    return tmp_dir

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name), 'r') as f:
      return f.read()

  def _run_optimize(self, input_args):
    # TODO(dbeam): make it possible to _run_optimize twice? Is that useful?
    args = input_args + [
        '--depfile',
        os.path.join(self._out_folder, 'depfile.d'),
        '--target_name',
        'dummy_target_name',
        '--input',
        self._tmp_src_dir,
        '--out_folder',
        self._out_folder,
    ]
    optimize_webui.main(args)

  def _write_v3_files_to_src_dir(self):
    self._write_file_to_src_dir('element.js', "alert('yay');")
    self._write_file_to_src_dir('element_in_dir/element_in_dir.js',
                                "alert('hello from element_in_dir');")
    self._write_file_to_src_dir(
        'ui.js', '''
import './element.js';
import './element_in_dir/element_in_dir.js';
''')
    self._write_file_to_src_dir(
        'ui.html', '''
<script type="module" src="ui.js"></script>
''')

  def _write_v3_files_with_custom_path_to_src_dir(self, custom_path):
    self._write_file_to_dir(
        os.path.join(custom_path, 'external_dir', 'external_element.js'), '''
import './sub_dir/external_element_dep.js';
alert('hello from external_element');
''')

    self._write_file_to_dir(
        os.path.join(custom_path, 'external_dir', 'sub_dir',
                     'external_element_dep.js'),
        "alert('hello from external_element_dep');")

    self._write_file_to_src_dir('element.js', "alert('yay');")
    self._write_file_to_src_dir(
        'ui.js', '''
import './element.js';
import 'some-fake-scheme://foo/external_dir/external_element.js';
''')
    self._write_file_to_src_dir(
        'ui.html', '''
<script type="module" src="ui.js"></script>
''')

  def _write_v3_files_with_resources_to_src_dir(self, resources_scheme):
    resources_path = os.path.join(
        _HERE_DIR.replace('\\', '/'), 'gen', 'ui', 'webui', 'resources', 'tsc',
        'js')
    fake_resource_path = os.path.join(resources_path, 'fake_resource.js')
    scheme_relative_resource_path = os.path.join(resources_path,
                                                 'scheme_relative_resource.js')
    os.makedirs(os.path.dirname(resources_path))

    self._tmp_dirs.append('gen')
    self._write_file_to_dir(
        fake_resource_path, '''
export const foo = 5;
alert('hello from shared resource');''')
    self._write_file_to_dir(
        scheme_relative_resource_path, '''
export const bar = 6;
alert('hello from another shared resource');''')

    self._write_file_to_src_dir(
        'element.js', '''
import '%s//resources/js/fake_resource.js';
import {bar} from '//resources/js/scheme_relative_resource.js';
alert('yay ' + bar);
''' % resources_scheme)
    self._write_file_to_src_dir(
        'element_in_dir/element_in_dir.js', '''
import {foo} from '%s//resources/js/fake_resource.js';
import '../strings.m.js';
alert('hello from element_in_dir ' + foo);
''' % resources_scheme)
    self._write_file_to_src_dir(
        'ui.js', '''
import './strings.m.js';
import './element.js';
import './element_in_dir/element_in_dir.js';
''')
    self._write_file_to_src_dir(
        'ui.html', '''
<script type="module" src="ui.js"></script>
''')

  def _check_output_html(self, out_html):
    self.assertNotIn('element.html', out_html)
    self.assertNotIn('element.js', out_html)
    self.assertNotIn('element_in_dir.html', out_html)
    self.assertNotIn('element_in_dir.js', out_html)
    self.assertIn('got here!', out_html)

  def _check_output_js(self, output_js_name):
    output_js = self._read_out_file(output_js_name)
    self.assertIn('yay', output_js)
    self.assertIn('hello from element_in_dir', output_js)

  def _check_output_depfile(self, has_html):
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    self.assertIn(
        os.path.normpath('element_in_dir/element_in_dir.js'), depfile_d)
    if (has_html):
      self.assertIn('element.html', depfile_d)
      self.assertIn(
          os.path.normpath('element_in_dir/element_in_dir.html'), depfile_d)

  def testV3SimpleOptimize(self):
    self._write_v3_files_to_src_dir()
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'ui.js',
    ]
    self._run_optimize(args)

    self._check_output_js('ui.rollup.js')
    self._check_output_depfile(False)

  def testV3OptimizeWithResources(self):
    self._write_v3_files_with_resources_to_src_dir('chrome:')
    resources_path = os.path.join('gen', 'ui', 'webui', 'resources', 'tsc')
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'ui.js',
        '--external_paths',
        'chrome://resources|%s' % resources_path,
    ]
    self._run_optimize(args)

    ui_rollup_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_rollup_js)
    self.assertIn('hello from element_in_dir', ui_rollup_js)
    self.assertIn('hello from shared resource', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    self.assertIn(
        os.path.normpath('element_in_dir/element_in_dir.js'), depfile_d)
    self.assertIn(
        os.path.normpath(
            '../gen/ui/webui/resources/tsc/js/scheme_relative_resource.js'),
        depfile_d)
    self.assertIn(
        os.path.normpath('../gen/ui/webui/resources/tsc/js/fake_resource.js'),
        depfile_d)

  def testV3MultiBundleOptimize(self):
    self._write_v3_files_to_src_dir()
    self._write_file_to_src_dir('lazy_element.js',
                                "alert('hello from lazy_element');")
    self._write_file_to_src_dir(
        'lazy.js', '''
import './lazy_element.js';
import './element_in_dir/element_in_dir.js';
''')

    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'ui.js',
        'lazy.js',
        '--out-manifest',
        os.path.join(self._out_folder, 'out_manifest.json'),
    ]
    self._run_optimize(args)

    # Check that the shared element is in the shared bundle and the non-shared
    # elements are in the individual bundles.
    ui_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_js)
    self.assertNotIn('hello from lazy_element', ui_js)
    self.assertNotIn('hello from element_in_dir', ui_js)

    lazy_js = self._read_out_file('lazy.rollup.js')
    self.assertNotIn('yay', lazy_js)
    self.assertIn('hello from lazy_element', lazy_js)
    self.assertNotIn('hello from element_in_dir', lazy_js)

    shared_js = self._read_out_file('shared.rollup.js')
    self.assertNotIn('yay', shared_js)
    self.assertNotIn('hello from lazy_element', shared_js)
    self.assertIn('hello from element_in_dir', shared_js)

    # All 3 JS files should be in the depfile.
    self._check_output_depfile(False)
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('lazy_element.js', depfile_d)

    manifest = json.loads(self._read_out_file('out_manifest.json'))
    self.assertEqual(3, len(manifest['files']))
    self.assertTrue('lazy.rollup.js' in manifest['files'])
    self.assertTrue('ui.rollup.js' in manifest['files'])
    self.assertTrue('shared.rollup.js' in manifest['files'])

    self.assertEqual(
        os.path.relpath(self._out_folder, _CWD).replace('\\', '/'),
        os.path.relpath(manifest['base_dir'], _CWD).replace('\\', '/'))

  def testV3OptimizeWithCustomPaths(self):
    custom_dir = os.path.join(self._create_tmp_dir(), 'foo_root')
    self._write_v3_files_with_custom_path_to_src_dir(custom_dir)
    resources_path = os.path.join('gen', 'ui', 'webui', 'resources', 'tsc')
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'ui.js',
        '--external_paths',
        'chrome://resources|%s' % resources_path,
        'some-fake-scheme://foo|%s' % os.path.abspath(custom_dir),
    ]
    self._run_optimize(args)

    ui_rollup_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_rollup_js)
    self.assertIn('hello from external_element', ui_rollup_js)
    self.assertIn('hello from external_element_dep', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    # Relative path from the src of the root module to the external root dir
    relpath = os.path.relpath(custom_dir, self._tmp_src_dir)
    self.assertIn(
        os.path.normpath(
            os.path.join(relpath, 'external_dir', 'external_element.js')),
        depfile_d)
    self.assertIn(
        os.path.normpath(
            os.path.join(relpath, 'external_dir', 'sub_dir',
                         'external_element_dep.js')), depfile_d)

  def testV3SimpleOptimizeExcludes(self):
    self._write_v3_files_to_src_dir()
    args = [
        '--host',
        'chrome-extension://myextensionid/',
        '--js_module_in_files',
        'ui.js',
        '--exclude',
        'element_in_dir/element_in_dir.js',
    ]
    self._run_optimize(args)

    output_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', output_js)
    self.assertNotIn('hello from element_in_dir', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    self.assertNotIn('element_in_dir', depfile_d)

  # Tests that bundling resources for an untrusted UI can successfully exclude
  # resources imported from both chrome-untrusted://resources and scheme
  # relative paths.
  def testV3SimpleOptimizeExcludesResources(self):
    self._write_v3_files_with_resources_to_src_dir('chrome-untrusted:')
    resources_path = os.path.join('gen', 'ui', 'webui', 'resources', 'tsc')
    args = [
        '--host',
        'chrome-untrusted://fake-host',
        '--js_module_in_files',
        'ui.js',
        '--external_paths',
        '//resources|%s' % resources_path,
        'chrome-untrusted://resources|%s' % resources_path,
        '--exclude',
        '//resources/js/scheme_relative_resource.js',
        'chrome-untrusted://resources/js/fake_resource.js',
    ]
    self._run_optimize(args)

    output_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', output_js)
    self.assertIn('//resources/js/scheme_relative_resource.js', output_js)
    self.assertIn('chrome-untrusted://resources/js/fake_resource.js', output_js)
    self.assertNotIn('hello from another shared resource', output_js)
    self.assertNotIn('hello from shared resource', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self.assertNotIn('fake_resource', depfile_d)
    self.assertNotIn('scheme_relative_resource', depfile_d)

  # Tests that bundling resources for an untrusted UI successfully bundles
  # resources from both chrome-untrusted://resources and //resources.
  def testV3SimpleOptimizeUntrustedResources(self):
    self._write_v3_files_with_resources_to_src_dir('chrome-untrusted:')
    resources_path = os.path.join('gen', 'ui', 'webui', 'resources', 'tsc')
    args = [
        '--host',
        'chrome-untrusted://fake-host',
        '--js_module_in_files',
        'ui.js',
        '--external_paths',
        '//resources|%s' % resources_path,
        'chrome-untrusted://resources|%s' % resources_path,
    ]
    self._run_optimize(args)

    output_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', output_js)
    self.assertIn('hello from another shared resource', output_js)
    self.assertIn('hello from shared resource', output_js)
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('fake_resource', depfile_d)
    self.assertIn('scheme_relative_resource', depfile_d)

  def testV3OptimizeWithCustomLayeredPaths(self):
    tmp_dir = self._create_tmp_dir()
    custom_dir_foo = os.path.join(tmp_dir, 'foo_root')
    custom_dir_bar = os.path.join(tmp_dir, 'bar_root')

    self._write_v3_files_with_custom_path_to_src_dir(custom_dir_foo)

    # Overwrite one of the foo files to import something from
    # some-fake-scheme://bar.
    self._write_file_to_dir(
        os.path.join(custom_dir_foo, 'external_dir', 'sub_dir',
                     'external_element_dep.js'), '''
import 'some-fake-scheme://bar/another_element.js';
alert('hello from external_element_dep');''')

    # Write that file to the bar_root directory.
    self._write_file_to_dir(
        os.path.join(custom_dir_bar, 'another_element.js'),
        "alert('hello from another external dep');")

    resources_path = os.path.join('gen', 'ui', 'webui', 'resources', 'tsc')
    args = [
        '--host',
        'fake-host',
        '--js_module_in_files',
        'ui.js',
        '--external_paths',
        '//resources|%s' % resources_path,
        'some-fake-scheme://foo|%s' % os.path.abspath(custom_dir_foo),
        'some-fake-scheme://bar|%s' % os.path.abspath(custom_dir_bar),
    ]
    self._run_optimize(args)

    ui_rollup_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_rollup_js)
    self.assertIn('hello from external_element', ui_rollup_js)
    self.assertIn('hello from external_element_dep', ui_rollup_js)
    self.assertIn('hello from another external dep', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    # Relative path from the src of the root module to the external root dir
    relpath = os.path.relpath(custom_dir_foo, self._tmp_src_dir)
    self.assertIn(
        os.path.normpath(
            os.path.join(relpath, 'external_dir', 'external_element.js')),
        depfile_d)
    self.assertIn(
        os.path.normpath(
            os.path.join(relpath, 'external_dir', 'sub_dir',
                         'external_element_dep.js')), depfile_d)
    # Relative path from the src of the root module to the secondary dependency
    # root dir.
    relpath_bar = os.path.relpath(custom_dir_bar, self._tmp_src_dir)
    self.assertIn(
        os.path.normpath(os.path.join(relpath_bar, 'another_element.js')),
        depfile_d)


if __name__ == '__main__':
  unittest.main()
