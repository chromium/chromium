#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_code_cache
import glob
import json
import os
import shutil
import sys  # noqa: F401
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)
_BUILD_DIR = os.environ['CHROMIUM_BUILD_DIRECTORY']


class GenerateCodeCacheTest(unittest.TestCase):
  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, 'r', encoding='utf-8') as file:
      return file.read()

  def _run_test(self, in_files, extra_args=None, out_dir=None):
    out_dir = out_dir or self._out_dir
    manifest_path = os.path.join(out_dir, "manifest.json")
    util_path = os.path.join(  # noqa: F841
      _HERE_DIR,
      "tests",
      "generate_code_cache",
      "code_cache_generator_test_util.py",
    )
    args = [
      "--util_path",
      os.path.join(_BUILD_DIR, "code_cache_generator"),
      "--in_folder",
      os.path.join(_HERE_DIR, "tests", "generate_code_cache"),
      "--in_files",
      *in_files,
      "--out_folder",
      out_dir,
      "--out_manifest",
      manifest_path,
      *(extra_args or []),
    ]

    generate_code_cache.main(args)

    manifest_data = json.loads(self._read_file(manifest_path))
    self.assertEqual(manifest_data['base_dir'], out_dir)

    for in_file in in_files:
      out_code_cache_file = f'{in_file}.code_cache'
      self.assertTrue(
        os.path.isfile(os.path.join(out_dir, out_code_cache_file))
      )
      self.assertTrue(out_code_cache_file in manifest_data['files'])

  def _get_snapshot_args(self):
    """Returns the flag that passes the build's own snapshot, or None."""
    context_snapshots = glob.glob(
      os.path.join(_BUILD_DIR, 'v8_context_snapshot*.bin')
    )
    if context_snapshots:
      return ['--context_snapshot', context_snapshots[0]]
    snapshot_blob = os.path.join(_BUILD_DIR, 'snapshot_blob.bin')
    if os.path.isfile(snapshot_blob):
      return ['--snapshot_blob', snapshot_blob]
    return None

  def testGenerateCodeCacheModuleBasic(self):
    self._run_test(["module_basic.js"])

  def testGenerateCodeCacheModuleWithDeps(self):
    self._run_test(["module_with_deps.js"])

  def testGenerateCodeCacheMultipleModules(self):
    self._run_test(["module_basic.js", "module_with_deps.js"])

  def testGenerateCodeCacheWithExplicitSnapshot(self):
    # Cross builds pass the target's snapshot explicitly. Passing the build's
    # own snapshot must give the same cache as letting the generator load it,
    # since the cache embeds the snapshot's checksum.
    snapshot_args = self._get_snapshot_args()
    if not snapshot_args:
      self.skipTest('No external V8 snapshot in the build directory')
    in_files = ["module_basic.js", "module_with_deps.js"]
    self._run_test(in_files)
    explicit_out_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self.addCleanup(shutil.rmtree, explicit_out_dir)
    self._run_test(in_files, snapshot_args, explicit_out_dir)
    for in_file in in_files:
      out_file = f'{in_file}.code_cache'
      with (
        open(os.path.join(self._out_dir, out_file), 'rb') as default_file,
        open(os.path.join(explicit_out_dir, out_file), 'rb') as explicit_file,
      ):
        self.assertEqual(default_file.read(), explicit_file.read(), out_file)

  def testSnapshotFlagsAreMutuallyExclusive(self):
    with self.assertRaises(SystemExit):
      self._run_test(
        ["module_basic.js"],
        [
          '--snapshot_blob',
          'snapshot_blob.bin',
          '--context_snapshot',
          'v8_context_snapshot.bin',
        ],
      )


if __name__ == "__main__":
  unittest.main()
