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
    gen_dir = os.path.join(
      self._out_folder, 'tools', 'typescript', 'tests', 'test_mappings'
    )
    path_mappings.main(
      [
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
      ]
      + self._additional_flags
    )
    return gen_dir

  def _assert_output(self, gen_dir, expected_file):
    output_file = 'path_mappings_test_mappings.json'
    self.assertTrue(os.path.exists(os.path.join(gen_dir, output_file)))
    expectations_dir = os.path.join(
      _HERE_DIR, 'tests', 'expected', 'path_mappings'
    )

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

  def testMojoManifests(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    out_rel = os.path.relpath(self._out_folder, _CWD).replace('\\', '/')

    shared_manifest = os.path.join(self._out_folder, 'shared_mojo.json')
    with open(shared_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': f'{out_rel}/ui/webui/resources/tsc/mojo',
              'dts_dir': f'{out_rel}/ui/webui/resources/tsc/mojo',
              'files': ['url/mojom/url.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    custom_manifest = os.path.join(self._out_folder, 'custom_mojo.json')
    with open(custom_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'entries': [
            {
              'base_dir': (
                f'{out_rel}/chrome/browser/ui/webui/foo/'
                'mojo_bindings_webui_minified'
              ),
              'dts_dir': (
                f'{out_rel}/chrome/browser/ui/webui/foo/mojo_bindings_webui_tsc'
              ),
              'import_path_prefix': 'chrome/browser/ui/webui/foo',
              'files': ['foo.mojom-webui.js'],
            }
          ]
        },
        f,
      )

    plain_ts_manifest = os.path.join(self._out_folder, 'plain_ts.json')
    with open(plain_ts_manifest, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'base_dir': f'{out_rel}/components/bar/tsc',
          'files': ['bar.js'],
        },
        f,
      )

    gen_dir = os.path.join(
      self._out_folder, 'tools', 'typescript', 'tests', 'test_mappings'
    )
    path_mappings.main(
      [
        '--root_gen_dir',
        os.path.relpath(self._out_folder, gen_dir),
        '--root_src_dir',
        os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
        '--gen_dir',
        os.path.relpath(gen_dir, _CWD),
        '--raw_deps',
        '//ui/webui/resources/mojo:build_ts',
        '--manifest_files',
        shared_manifest,
        custom_manifest,
        plain_ts_manifest,
        # Passing a duplicate identical manifest entry should be deduplicated.
        shared_manifest,
        '--webui_context_type',
        'trusted_only',
        '--output_suffix',
        'test_mappings',
        '--pretty_print',
      ]
    )

    output_file = os.path.join(gen_dir, 'path_mappings_test_mappings.json')
    with open(output_file, 'r', encoding='utf-8') as f:
      actual = json.load(f)

    self.assertEqual(
      ['./../../../../ui/webui/resources/tsc/mojo/*'],
      actual['chrome://resources/mojo/*'],
    )
    self.assertNotIn('//resources/mojo/*', actual)
    self.assertEqual(
      [
        './../../../../ui/webui/resources/tsc/mojo/'
        'url/mojom/url.mojom-webui.d.ts'
      ],
      actual['url/mojom/url.mojom-webui.js'],
    )
    self.assertEqual(
      [
        './../../../../chrome/browser/ui/webui/foo/mojo_bindings_webui_tsc/'
        'foo.mojom-webui.d.ts'
      ],
      actual['chrome/browser/ui/webui/foo/foo.mojom-webui.js'],
    )
    self.assertEqual(
      ['./../../../../components/bar/tsc/bar.d.ts'],
      actual['bar.js'],
    )

  def testConflictingManifestMappingsFails(self):
    self._out_folder = tempfile.mkdtemp(dir=_CWD)
    out_rel = os.path.relpath(self._out_folder, _CWD).replace('\\', '/')

    manifest_1 = os.path.join(self._out_folder, 'manifest_1.json')
    with open(manifest_1, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'base_dir': f'{out_rel}/components/foo/tsc',
          'files': ['shared_name.js'],
        },
        f,
      )

    manifest_2 = os.path.join(self._out_folder, 'manifest_2.json')
    with open(manifest_2, 'w', encoding='utf-8') as f:
      json.dump(
        {
          'base_dir': f'{out_rel}/components/bar/tsc',
          'files': ['shared_name.js'],
        },
        f,
      )

    gen_dir = os.path.join(
      self._out_folder, 'tools', 'typescript', 'tests', 'test_mappings'
    )
    with self.assertRaisesRegex(AssertionError, 'Conflicting path mappings'):
      path_mappings.main(
        [
          '--root_gen_dir',
          os.path.relpath(self._out_folder, gen_dir),
          '--root_src_dir',
          os.path.relpath(os.path.join(_HERE_DIR, 'tests'), gen_dir),
          '--gen_dir',
          os.path.relpath(gen_dir, _CWD),
          '--manifest_files',
          manifest_1,
          manifest_2,
          '--output_suffix',
          'test_mappings',
        ]
      )


if __name__ == '__main__':
  unittest.main()
