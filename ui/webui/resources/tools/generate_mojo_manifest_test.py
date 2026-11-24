#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

import generate_mojo_manifest

_HERE_DIR = os.path.dirname(__file__)


class GenerateMojoManifestTest(unittest.TestCase):
  def setUp(self):
    self._temp_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def testGenerateMojoManifest(self):
    dep_manifest_1 = os.path.join(self._temp_dir, 'dep1.json')
    with open(dep_manifest_1, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/foo/mojo_bindings_webui_minified',
              'dts_dir': 'gen/foo/mojo_bindings_webui_tsc',
              'import_path_prefix': 'foo',
              'files': ['a.mojom-webui.js', 'b.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    dep_manifest_2 = os.path.join(self._temp_dir, 'dep2.json')
    with open(dep_manifest_2, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/foo/mojo_bindings_webui_minified',
              'dts_dir': 'gen/foo/mojo_bindings_webui_tsc',
              'import_path_prefix': 'foo',
              'files': ['b.mojom-webui.js'],
            },
            {
              'base_dir': 'gen/bar/mojom_webui_minified',
              'dts_dir': 'gen/bar/mojom_webui_tsc',
              'import_path_prefix': 'bar',
              'files': ['c.mojom-webui.js'],
            },
          ]
        },
        f,
      )

    out_manifest = os.path.join(self._temp_dir, 'out_manifest.json')
    generate_mojo_manifest.main(
      [
        '--base_dir',
        'gen/baz/mojo_bindings_webui_minified',
        '--dts_dir',
        'gen/baz/mojo_bindings_webui_tsc',
        '--import_path_prefix',
        'baz',
        '--out_manifest',
        out_manifest,
        '--dep_manifests',
        dep_manifest_1,
        dep_manifest_2,
        '--in_files',
        'd.mojom-webui.js',
      ]
    )

    with open(out_manifest, 'r', encoding='utf-8') as f:
      data = json.load(f)

    self.assertEqual(
      {
        'entries': [
          {
            'base_dir': 'gen/foo/mojo_bindings_webui_minified',
            'dts_dir': 'gen/foo/mojo_bindings_webui_tsc',
            'import_path_prefix': 'foo',
            'files': ['a.mojom-webui.js', 'b.mojom-webui.js'],
          },
          {
            'base_dir': 'gen/bar/mojom_webui_minified',
            'dts_dir': 'gen/bar/mojom_webui_tsc',
            'import_path_prefix': 'bar',
            'files': ['c.mojom-webui.js'],
          },
          {
            'base_dir': 'gen/baz/mojo_bindings_webui_minified',
            'dts_dir': 'gen/baz/mojo_bindings_webui_tsc',
            'import_path_prefix': 'baz',
            'files': ['d.mojom-webui.js'],
          },
        ]
      },
      data,
    )

  def testConflictingManifestEntriesFails(self):
    dep_manifest = os.path.join(self._temp_dir, 'dep.json')
    with open(dep_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/other_tsc',
              'dts_dir': 'gen/other_tsc',
              'import_path_prefix': 'foo',
              'files': ['a.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    out_manifest = os.path.join(self._temp_dir, 'out_manifest.json')
    with self.assertRaisesRegex(AssertionError, 'Conflicting manifest entries'):
      generate_mojo_manifest.main(
        [
          '--base_dir',
          'gen/foo/mojo_bindings_webui_tsc',
          '--import_path_prefix',
          'foo',
          '--out_manifest',
          out_manifest,
          '--dep_manifests',
          dep_manifest,
          '--in_files',
          'a.mojom-webui.js',
        ]
      )

  def testTsLibraryStyleDepManifest(self):
    ts_dep_manifest = os.path.join(self._temp_dir, 'ts_dep.json')
    with open(ts_dep_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'base_dir': 'gen/components/foo/tsc',
          'files': ['helper.js'],
        },
        f,
      )

    out_manifest = os.path.join(self._temp_dir, 'out_manifest.json')
    generate_mojo_manifest.main(
      [
        '--base_dir',
        'gen/bar/mojo_bindings_webui_tsc',
        '--import_path_prefix',
        'bar',
        '--out_manifest',
        out_manifest,
        '--dep_manifests',
        ts_dep_manifest,
        '--in_files',
        'bar.mojom-webui.js',
      ]
    )

    with open(out_manifest, 'r', encoding='utf-8') as f:
      data = json.load(f)

    self.assertEqual(
      {
        'entries': [
          {
            'base_dir': 'gen/components/foo/tsc',
            'dts_dir': 'gen/components/foo/tsc',
            'files': ['helper.js'],
          },
          {
            'base_dir': 'gen/bar/mojo_bindings_webui_tsc',
            'dts_dir': 'gen/bar/mojo_bindings_webui_tsc',
            'import_path_prefix': 'bar',
            'files': ['bar.mojom-webui.js'],
          },
        ]
      },
      data,
    )

  def testAggregationOnlyAndImportPrefixNormalization(self):
    dep_manifest_1 = os.path.join(self._temp_dir, 'dep1.json')
    with open(dep_manifest_1, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen/foo/tsc/',
              'dts_dir': 'gen/foo/tsc/',
              'import_path_prefix': '/foo/bar/',
              'files': ['sub/a.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    dep_manifest_2 = os.path.join(self._temp_dir, 'dep2.json')
    with open(dep_manifest_2, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': 'gen\\foo\\tsc\\',
              'dts_dir': 'gen\\foo\\tsc\\',
              'import_path_prefix': '\\foo\\bar\\',
              'files': ['sub\\a.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    out_manifest = os.path.join(self._temp_dir, 'out_manifest.json')
    generate_mojo_manifest.main(
      [
        '--out_manifest',
        out_manifest,
        '--dep_manifests',
        dep_manifest_1,
        dep_manifest_2,
      ]
    )

    with open(out_manifest, 'r', encoding='utf-8') as f:
      data = json.load(f)

    self.assertEqual(
      {
        'entries': [
          {
            'base_dir': 'gen/foo/tsc',
            'dts_dir': 'gen/foo/tsc',
            'import_path_prefix': 'foo/bar',
            'files': ['sub/a.mojom-webui.js'],
          },
        ]
      },
      data,
    )

  def testServePathPrefix(self):
    out_manifest = os.path.join(self._temp_dir, 'shared_manifest.json')
    generate_mojo_manifest.main(
      [
        '--base_dir',
        'gen/ui/webui/resources/tsc/mojo',
        '--serve_path_prefix',
        '//resources/mojo/',
        '--out_manifest',
        out_manifest,
        '--in_files',
        'url/mojom/url.mojom-webui.js',
      ]
    )

    with open(out_manifest, 'r', encoding='utf-8') as f:
      data = json.load(f)

    self.assertEqual(
      {
        'entries': [
          {
            'base_dir': 'gen/ui/webui/resources/tsc/mojo',
            'dts_dir': 'gen/ui/webui/resources/tsc/mojo',
            'serve_path_prefix': '//resources/mojo',
            'files': ['url/mojom/url.mojom-webui.js'],
          },
        ]
      },
      data,
    )

    # Merging a manifest with the same import path but a different
    # serve_path_prefix must fail.
    conflicting_manifest = os.path.join(self._temp_dir, 'conflict.json')
    with self.assertRaisesRegex(AssertionError, 'Conflicting manifest entries'):
      generate_mojo_manifest.main(
        [
          '--base_dir',
          'gen/ui/webui/resources/tsc/mojo',
          '--serve_path_prefix',
          '//resources/other',
          '--out_manifest',
          conflicting_manifest,
          '--dep_manifests',
          out_manifest,
          '--in_files',
          'url/mojom/url.mojom-webui.js',
        ]
      )


if __name__ == '__main__':
  unittest.main()
