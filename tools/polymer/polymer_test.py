#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import polymer
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class PolymerModulizerTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._additional_flags = []

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'rb').read()

  def _run_test(self, html_type, html_file, js_file,
      js_out_file, js_file_expected):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    polymer.main([
      '--in_folder', os.path.join(_HERE_DIR, 'tests'),
      '--out_folder', self._out_folder,
      '--js_file',  js_file,
      '--html_file',  html_file,
      '--html_type',  html_type,
      '--namespace_rewrites',
      'Polymer.PaperRippleBehavior|PaperRippleBehavior',
      '--auto_imports',
      'ui/webui/resources/html/polymer.html|Polymer,html',
      'third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-ripple-behavior.html|PaperRippleBehavior',
    ] + self._additional_flags)

    actual_js = self._read_out_file(js_out_file)
    expected_js = open(os.path.join(
        _HERE_DIR, 'tests', js_file_expected), 'rb').read()
    self.assertEqual(expected_js.split(b'\n'), actual_js.split(b'\n'))

  # Test case where HTML is extracted from a Polymer2 <dom-module>.
  def testDomModule(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module.js',
        'dom_module.m.js', 'dom_module_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that is
  # using ES6 class syntax.
  def testDomModuleWithClassSyntax(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module_with_class_syntax.js',
        'dom_module_with_class_syntax.m.js', 'dom_module_with_class_syntax_expected.js')

  # Test case where a commented out HTML import exists in the original HTML
  # file. It is purposefully picked up and converted to a JS module, to address
  # a unique use case of the FilesApp where an HTML import does not actually
  # exist in the Polymer2 code.
  # TODO(crbug.com/1133186): Remove after FilesApp Polymer3 migration is
  # completed.
  def testDomModuleWithCommentedOutImport(self):
    self._run_test('dom-module', 'dom_module_with_commented_out_import.html',
                   'dom_module.js', 'dom_module.m.js',
                   'dom_module_with_commented_out_import_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that is
  # wrapped in an IIFE function.
  def testDomModuleIife(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module_iife.js',
        'dom_module_iife.m.js', 'dom_module_iife_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that is
  # wrapped in an arrow IIFE function.
  def testDomModuleIifeArrow(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module_iife_arrow.js',
        'dom_module_iife_arrow.m.js', 'dom_module_iife_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that is
  # assigned to a variable.
  def testDomModuleIifeAndAssigned(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module_with_assignment.js',
        'dom_module_with_assignment.m.js',
        'dom_module_with_assignment_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that also
  # has a 'cr.define()' in its JS file.
  def testDomModuleWithDefine(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module_with_define.js',
        'dom_module_with_define.m.js', 'dom_module_with_define_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that has
  # ignore annotations.
  def testDomModuleWithIgnore(self):
    self._run_test('dom-module', 'dom_module.html', 'dom_module_with_ignore.js',
                   'dom_module_with_ignore.m.js',
                   'dom_module_with_ignore_expected.js')

  # Test case where some HTML imports should be ignored.
  def testDomModuleWithIgnoreImports(self):
    self._additional_flags = [
      '--ignore_imports',
      'ui/webui/resources/html/ignore_me.html',
    ]
    self._run_test('dom-module', 'dom_module.html', 'dom_module.js',
                   'dom_module.m.js',
                   'dom_module_with_ignore_imports_expected.js')

  # Test case where some HTML imports have already been fully migrated to
  # Polymer3.
  def testDomModuleWithMigratedImports(self):
    self._additional_flags = [
      '--migrated_imports',
      'tools/polymer/tests/foo.html',
      'ui/webui/resources/html/ignore_me.html',
    ]
    self._run_test('dom-module', 'dom_module.html', 'dom_module.js',
                   'dom_module.m.js',
                   'dom_module_with_migrated_imports_expected.js')

  # Test case where HTML is extracted from a Polymer2 <dom-module> that also
  # uses <if expr> for imports.
  def testDomModuleWithConditionalImport(self):
    self._run_test('dom-module', 'dom_module_with_if_expr.html',
                   'dom_module.js', 'dom_module.m.js',
                   'dom_module_with_if_expr_expected.js')

  # Test case where HTML has some comment before the first <link rel="import"> \
  # and also uses <if expr> for imports.
  def testDomModuleImportsWithCopyrightPrefix(self):
    self._run_test('dom-module', 'dom_module_with_copyright.html',
                   'dom_module.js', 'dom_module.m.js',
                   'dom_module_with_if_expr_expected.js')

  # Test case where HTML is extracted from a Polymer2 style module.
  def testStyleModule(self):
    self._run_test(
        'style-module', 'style_module.html', 'style_module.m.js',
        'style_module.m.js', 'style_module_expected.js')
    return

  # Test case where HTML is extracted from a Polymer2 <custom-style>.
  def testCustomStyle(self):
    self._run_test(
        'custom-style', 'custom_style.html', 'custom_style.m.js',
        'custom_style.m.js', 'custom_style_expected.js')

  # Test case where HTML is extracted from a Polymer2 iron-iconset-svg file.
  def testIronIconset(self):
    self._run_test(
        'iron-iconset', 'iron_iconset.html', 'iron_iconset.m.js',
        'iron_iconset.m.js', 'iron_iconset_expected.js')

  # Test case where the provided HTML is already in the form needed by Polymer3.
  def testV3Ready(self):
    self._run_test(
        'v3-ready', 'v3_ready.html', 'v3_ready.js',
        'v3_ready.js', 'v3_ready_expected.js')


  # Test the |Dependency| class directly, which is responsible for converting
  # HTML imports to JS imports.
  def testImportsHtmlToJs(self):
    _HERE = os.path.abspath(os.path.dirname(__file__))
    _ROOT = os.path.normpath(os.path.join(_HERE, '..', '..'))

    src = os.path.join(_ROOT, 'ui/webui/resources/foo/bar/baz.html')

    auto_imports = {
      'ui/webui/resources/html/polymer.html': ['Polymer', 'html'],
      'ui/webui/resources/html/foo.html': ['Foo'],
    }

    def assert_html_to_js(html, expected_js):
      actual_js = polymer.Dependency(src, html).to_js_import(auto_imports)
      self.assertEqual(expected_js, actual_js)

    cases = [
        # Relative paths cases.
        # Case where relative path to polymer.html is used.
        [
            '../../html/polymer.html',
            'import {Polymer, html} from \'//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
            'import {Polymer, html} from \'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
        ],
        # Case where relative path to file in the same folder is used.
        [
            'foo.html',
            'import \'./foo.m.js\';',
            'import \'./foo.m.js\';',
        ],
        # Case where relative path to file in the same subtree is used.
        [
            'path/to/subfolder/foo.html',
            'import \'./path/to/subfolder/foo.m.js\';',
            'import \'./path/to/subfolder/foo.m.js\';',
        ],
        # Case where relative path to file in ui/webui/resources/html/ is used.
        [
            '../../html/foo.html',
            'import {Foo} from \'../../js/foo.m.js\';',
            'import {Foo} from \'../../js/foo.m.js\';',
        ],

        # chrome:// paths cases.
        # Case where absolute path to a Polymer UI element is used.
        [
            'chrome://resources/polymer/v1_0/path/to/folder/foo.html',
            'import \'//resources/polymer/v3_0/path/to/folder/foo.js\';',
            'import \'chrome://resources/polymer/v3_0/path/to/folder/foo.js\';',
        ],
        # Case where chrome:// path to polymer.html is used.
        [
            'chrome://resources/html/polymer.html',
            'import {Polymer, html} from \'//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
            'import {Polymer, html} from \'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
        ],
        # Case where chrome://resources/html/ path to something other than
        # polymer.html is used.
        [
            'chrome://resources/html/bar.html',
            'import \'//resources/js/bar.m.js\';',
            'import \'chrome://resources/js/bar.m.js\';',
        ],

        # chrome-extension:// paths cases.
        [
            'chrome-extension://path/to/folder/foo.html',
            'import \'//path/to/folder/foo.m.js\';',
            'import \'chrome-extension://path/to/folder/foo.m.js\';',
        ],

        # Scheme-relative paths cases.
        # Case where absolute path to a Polymer UI element is used.
        [
            '//resources/polymer/v1_0/path/to/folder/foo.html',
            'import \'//resources/polymer/v3_0/path/to/folder/foo.js\';',
            'import \'//resources/polymer/v3_0/path/to/folder/foo.js\';',
        ],
        # Case where path to polymer.html is used.
        [
            '//resources/html/polymer.html',
            'import {Polymer, html} from \'//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
            'import {Polymer, html} from \'//resources/polymer/v3_0/polymer/polymer_bundled.min.js\';',
        ],
        # Case where //resources/html/ path to something other than
        # polymer.html is used.
        [
            '//resources/html/bar.html',
            'import \'//resources/js/bar.m.js\';',
            'import \'//resources/js/bar.m.js\';',
        ],
    ]

    for [html, js_expected1, js_expected2] in cases:
      # Test case where |preserve_url_scheme| is False
      polymer._preserve_url_scheme = False
      assert_html_to_js(html, js_expected1)

      # Test case where |preserve_url_scheme| is True
      polymer._preserve_url_scheme = True
      assert_html_to_js(html, js_expected2)


if __name__ == '__main__':
  unittest.main()
