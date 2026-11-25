#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

import generate_import_map

_HERE_DIR = os.path.dirname(__file__)


class GenerateImportMapTest(unittest.TestCase):
  def setUp(self):
    self._temp_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def testGenerateImportMap(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(os.path.join(in_dir, 'sub'))

    with open(os.path.join(in_dir, 'index.html'), 'w', encoding='utf-8') as f:
      f.write('<!DOCTYPE html><html><head><title>Test</title></head></html>')
    with open(
      os.path.join(in_dir, 'sub', 'dialog.html'), 'w', encoding='utf-8'
    ) as f:
      f.write('<!DOCTYPE html><html><head><title>Dialog</title></head></html>')

    shared_manifest = os.path.join(self._temp_dir, 'shared_mojo.json')
    with open(shared_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/ui/webui/resources/tsc/mojo',
              'dts_dir': 'gen/ui/webui/resources/tsc/mojo',
              'serve_path_prefix': '//resources/mojo',
              'files': ['url/mojom/url.mojom-webui.js'],
            },
            {
              'base_dir': 'gen/ui/webui/resources/tsc/js',
              'dts_dir': 'gen/ui/webui/resources/tsc/js',
              'import_path_prefix': 'ui/webui/resources/js',
              'serve_path_prefix': '//resources/js',
              'files': ['assert.js'],
            },
          ]
        },
        f,
      )

    page_manifest = os.path.join(self._temp_dir, 'page_mojo.json')
    with open(page_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': (
                'gen/chrome/browser/ui/webui/foo/mojo_bindings_webui_tsc'
              ),
              'import_path_prefix': 'chrome/browser/ui/webui/foo',
              'files': ['foo.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    out_manifest = os.path.join(self._temp_dir, 'out_manifest.json')
    generate_import_map.main(
      [
        '--in_folder',
        in_dir,
        '--out_folder',
        out_dir,
        '--in_files',
        'index.html',
        'sub/dialog.html',
        '--out_manifest',
        out_manifest,
        '--manifest_files',
        shared_manifest,
        page_manifest,
        # Passing the same manifest twice should deduplicate cleanly.
        page_manifest,
      ]
    )

    expected_map = json.dumps(
      {
        'imports': {
          'url/mojom/url.mojom-webui.js': (
            '//resources/mojo/url/mojom/url.mojom-webui.js'
          ),
          'ui/webui/resources/js/assert.js': '//resources/js/assert.js',
          'chrome/browser/ui/webui/foo/foo.mojom-webui.js': (
            '/chrome/browser/ui/webui/foo/foo.mojom-webui.js'
          ),
        }
      }
    )

    expected_script_tag = f'<script type="importmap">{expected_map}</script>'
    with open(os.path.join(out_dir, 'index.html'), 'r', encoding='utf-8') as f:
      out_html = f.read()
    self.assertEqual(
      '<!DOCTYPE html><html><head><title>Test</title>'
      f'{expected_script_tag}\n</head></html>',
      out_html,
    )

    with open(
      os.path.join(out_dir, 'sub', 'dialog.html'), 'r', encoding='utf-8'
    ) as f:
      dialog_html = f.read()
    self.assertEqual(
      '<!DOCTYPE html><html><head><title>Dialog</title>'
      f'{expected_script_tag}\n</head></html>',
      dialog_html,
    )

    with open(
      os.path.join(out_dir, 'test_loader.html'), 'r', encoding='utf-8'
    ) as f:
      test_loader_html = f.read()
    self.assertEqual(
      '<!DOCTYPE html>\n'
      '<html>\n'
      '<head>\n'
      f'  {expected_script_tag}\n'
      '</head>\n'
      '<body>\n'
      '  <script src="//webui-test/mocha.js"></script>\n'
      '  <script type="module" src="test_loader.js"></script>\n'
      '</body>\n'
      '</html>',
      test_loader_html,
    )

    with open(out_manifest, 'r', encoding='utf-8') as f:
      manifest_data = json.load(f)
    self.assertEqual(
      {
        'base_dir': out_dir.replace('\\', '/'),
        'files': ['index.html', 'sub/dialog.html', 'test_loader.html'],
      },
      manifest_data,
    )

  def testServePathPrefixRootAndValidation(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(in_dir)

    with open(os.path.join(in_dir, 'index.html'), 'w', encoding='utf-8') as f:
      f.write('<!DOCTYPE html><html><head></head></html>')

    root_manifest = os.path.join(self._temp_dir, 'root_serve.json')
    with open(root_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/foo',
              'import_path_prefix': 'chrome/browser/foo',
              'serve_path_prefix': '/',
              'files': ['bar.js'],
            }
          ]
        },
        f,
      )

    generate_import_map.main(
      [
        '--in_folder',
        in_dir,
        '--out_folder',
        out_dir,
        '--in_files',
        'index.html',
        '--manifest_files',
        root_manifest,
      ]
    )

    expected_map = json.dumps(
      {'imports': {'chrome/browser/foo/bar.js': '/bar.js'}}
    )
    with open(os.path.join(out_dir, 'index.html'), 'r', encoding='utf-8') as f:
      out_html = f.read()
    self.assertEqual(
      f'<!DOCTYPE html><html><head><script type="importmap">{expected_map}'
      '</script>\n</head></html>',
      out_html,
    )

    invalid_manifest = os.path.join(self._temp_dir, 'invalid_serve.json')
    with open(invalid_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/foo',
              'serve_path_prefix': 'resources/mojo',
              'files': ['bar.js'],
            }
          ]
        },
        f,
      )

    with self.assertRaisesRegex(AssertionError, 'Invalid serve_path_prefix'):
      generate_import_map.main(
        [
          '--in_folder',
          in_dir,
          '--out_folder',
          out_dir,
          '--in_files',
          'index.html',
          '--manifest_files',
          invalid_manifest,
        ]
      )

  def testMissingHeadTagFails(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(in_dir)

    html_path = os.path.join(in_dir, 'index.html')
    with open(html_path, 'w', encoding='utf-8') as f:
      f.write(
        '<!DOCTYPE html><html><!-- <head></head> --><body>No head</body></html>'
      )

    with self.assertRaises(AssertionError):
      generate_import_map.main(
        [
          '--in_folder',
          in_dir,
          '--out_folder',
          out_dir,
          '--in_files',
          'index.html',
        ]
      )

  def testConflictingImportMapEntriesFails(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(in_dir)

    html_path = os.path.join(in_dir, 'index.html')
    with open(html_path, 'w', encoding='utf-8') as f:
      f.write('<!DOCTYPE html><html><head></head></html>')

    shared_manifest = os.path.join(self._temp_dir, 'shared_mojo.json')
    with open(shared_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/ui/webui/resources/tsc/mojo',
              'serve_path_prefix': '//resources/mojo',
              'files': ['url/mojom/url.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    conflicting_manifest = os.path.join(self._temp_dir, 'conflicting.json')
    with open(conflicting_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/url/mojom/tsc',
              'import_path_prefix': 'url/mojom',
              'files': ['url.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    with self.assertRaisesRegex(AssertionError, 'Conflicting import map'):
      generate_import_map.main(
        [
          '--in_folder',
          in_dir,
          '--out_folder',
          out_dir,
          '--in_files',
          'index.html',
          '--manifest_files',
          shared_manifest,
          conflicting_manifest,
        ]
      )

  def testScriptInHeadInjectsBeforeFirstScript(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(in_dir)

    html_path = os.path.join(in_dir, 'index.html')
    with open(html_path, 'w', encoding='utf-8') as f:
      f.write(
        '<!doctype html><html><head>\n'
        '  <!-- <script src="ignored_comment.js"></script> -->\n'
        '  <meta charset="utf-8">\n'
        '  <base href="chrome://foo">\n'
        '  <script type="module" src="app.js"></script>\n'
        '</head><body></body></html>'
      )

    generate_import_map.main(
      [
        '--in_folder',
        in_dir,
        '--out_folder',
        out_dir,
        '--in_files',
        'index.html',
      ]
    )

    with open(os.path.join(out_dir, 'index.html'), 'r', encoding='utf-8') as f:
      out_html = f.read()

    importmap_tag = '<script type="importmap">{"imports": {}}</script>'
    self.assertIn(importmap_tag, out_html)
    self.assertLess(
      out_html.index('<base href="chrome://foo">'),
      out_html.index(importmap_tag),
    )
    self.assertLess(
      out_html.index(importmap_tag),
      out_html.index('<script type="module" src="app.js"></script>'),
    )

  def testModulepreloadInHeadInjectsBeforeLink(self):
    in_dir = os.path.join(self._temp_dir, 'in')
    out_dir = os.path.join(self._temp_dir, 'out')
    os.makedirs(in_dir)

    html_path = os.path.join(in_dir, 'index.html')
    with open(html_path, 'w', encoding='utf-8') as f:
      f.write(
        '<!doctype html><html><head>\n'
        '  <meta charset="utf-8">\n'
        '  <link rel="stylesheet" href="app.css">\n'
        '  <link rel="modulepreload" href="screens.js">\n'
        '</head><body><script type="module" src="app.js"></script></body>'
        '</html>'
      )

    generate_import_map.main(
      [
        '--in_folder',
        in_dir,
        '--out_folder',
        out_dir,
        '--in_files',
        'index.html',
      ]
    )

    with open(os.path.join(out_dir, 'index.html'), 'r', encoding='utf-8') as f:
      out_html = f.read()

    importmap_tag = '<script type="importmap">{"imports": {}}</script>'
    self.assertIn(importmap_tag, out_html)
    self.assertLess(
      out_html.index('<link rel="stylesheet" href="app.css">'),
      out_html.index(importmap_tag),
    )
    self.assertLess(
      out_html.index(importmap_tag),
      out_html.index('<link rel="modulepreload" href="screens.js">'),
    )


if __name__ == '__main__':
  unittest.main()
