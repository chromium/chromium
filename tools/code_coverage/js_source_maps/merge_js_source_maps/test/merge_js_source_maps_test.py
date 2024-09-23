#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import shutil
import sys
import tempfile
import unittest

from pathlib import Path

_HERE_DIR = Path(__file__).parent.resolve()
_SOURCE_MAP_MERGER = (_HERE_DIR.parent / 'merge_js_source_maps.js').resolve()

# TODO(crbug.com/40229311): Move common sourcemap build rules and tests into a
# more generic location.
_SOURCE_MAP_TRANSLATOR = (_HERE_DIR.parent.parent / 'create_js_source_maps' /
                          'test' / 'translate_source_map.js').resolve()

_NODE_PATH = (_HERE_DIR.parent.parent.parent.parent.parent / 'third_party' /
              'node').resolve()
sys.path.append(str(_NODE_PATH))

import node

_SOURCE_MAPPING_DATA_URL_PREFIX = \
    '//# sourceMappingURL=data:application/json;base64,'


class MergeSourceMapsTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _translate(self, source_map, line, column):
    """ Translates from post-transform to pre-transform using a source map.

    Translates a line and column in some hypothetical processed JavaScript
    back into the hypothetical original line and column using the indicated
    source map. Returns the pre-processed line and column.
    """
    stdout = node.RunNode([
        str(_SOURCE_MAP_TRANSLATOR), "--source_map", source_map, "--line",
        str(line), "--column",
        str(column)
    ])
    result = json.loads(stdout)
    return result['line'], result['column']

  def _writeResponseFileContents(self, sources, outputs, manifest_file):
    with tempfile.NamedTemporaryFile(mode='w+',
                                     dir=self._out_folder,
                                     delete=False) as response_file:
      if manifest_file:
        response_file.write('--manifest-files ')
        response_file.write(manifest_file + ' ')
      if sources:
        response_file.write('--sources ')
        response_file.write(str(sources.resolve()) + ' ')
      if outputs:
        response_file.write('--outputs ')
        response_file.write(str(outputs.resolve()) + ' ')
      return response_file.name

  def testMergingTwoSourcemaps(self):
    """ Test that merging 2 inline sourcemaps results in line numbers and
    columns that refer to positions in the original file.

    The file `original_file.ts` contains a mix of GRiT <if expr> that get
    removed and some TS specific properties that get rewritten to JS. These have
    been ran through both `create_js_source_maps` build rule AND tsc to provide
    a generated file with 2 inline sourcemaps.
    """
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    original_file_name = 'original_file.ts'
    input_file_name = (_HERE_DIR / 'generated_file_pre_merge.js').resolve()
    output_file_name = (Path(self._out_folder) / "merged_maps.out").resolve()
    response_file_name = self._writeResponseFileContents(
        input_file_name, output_file_name, None)
    node.RunNode([
        str(_SOURCE_MAP_MERGER),
        '--response-file-name',
        str(response_file_name),
        '--out-dir',
        'tsc',
    ])

    source_map = None
    num_sourcemaps = 0
    with open(output_file_name, encoding='utf-8') as f:
      merged_source_map_lines = f.readlines()
      for line in merged_source_map_lines:
        stripped_line = line.strip()
        if stripped_line.startswith(_SOURCE_MAPPING_DATA_URL_PREFIX):
          source_map = base64.b64decode(
              stripped_line.replace(_SOURCE_MAPPING_DATA_URL_PREFIX,
                                    '')).decode('utf-8')
          num_sourcemaps += 1

    # In the event of an error (in both the source map pipeline and/or merging)
    # the original file is simply copied over to the new directory to get out of
    # the way of subsequent steps. This will leave multiple inlined sourcemaps,
    # verify the merging actually succeeded before continuing.
    if num_sourcemaps != 1:
      self.fail('Number of sourcemaps invalid, got: %d, want: 1' %
                num_sourcemaps)

    # The sources key must refer to the original file name.
    self.assertEqual(original_file_name, json.loads(source_map)['sources'][0])

    # Check various mappings.
    # The first line should not have an equivalent mapping:
    # "use strict";
    line, column = self._translate(source_map, 1, 1)
    self.assertEqual(line, None)

    # The IIFE representing the enum should map to the line, this large jump in
    # line numbers represents the GRiT <if expr> that has been removed.
    line, column = self._translate(source_map, 7, 1)
    self.assertEqual(line, 12)

    # Certain things remain the same, such as console.log and these should just
    # get their line / column repointed to the new location.
    line, column = self._translate(source_map, 27, 8)
    self.assertEqual(line, 31)
    self.assertEqual(column, 0)

    # Expect the sources array to have both the intermediate file and original
    # file. When remapping the VLQ mappings reference only the original file but
    # in the case some explicit mappings happen in the intermediate file it is
    # included.
    json_source_map = json.loads(source_map)
    self.assertEqual(len(json_source_map['sources']), 2)

    source_idx = json_source_map['sources'].index(original_file_name)

    with open(os.path.join(_HERE_DIR, original_file_name),
              encoding='utf-8') as f:
      original_file_lines = f.readlines()
      source_file_lines = json_source_map['sourcesContent'][source_idx].split(
          '\n')
      for line_num in range(len(original_file_lines)):
        stripped_original_line = original_file_lines[line_num].strip()
        stripped_source_line = source_file_lines[line_num].strip()
        # TODO(b:265973389) The sourcemap was generated with old license and
        # hence will have the old license header. Remove this
        # once we have an updated sourcemap.
        if (stripped_original_line == "// Copyright 2022 The Chromium Authors"):
          stripped_original_line = (stripped_original_line +
                                    ". All rights reserved.")

        # Verify line by line that the sourceContent matches the actual source.
        # This ensures the file are appropriately passed along the pipeline.
        self.assertEqual(stripped_original_line, stripped_source_line)

  def testManifestFilesAreRewritten(self):
    """ Tests that when `manifest-files` are supplied the results are rewritten
    to reflect the updated files location.

    This is critical to ensure any files that undergo a merge (or move) are
    appropriately rewritten and subsequent steps can rely on the correct merged
    file instead of the previous unmerged one.
    """
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)

    manifest_file_contents = b'{"base_dir":"tsc/ts_library_out"}'
    input_fd, manifest_file = tempfile.mkstemp(dir=self._out_folder,
                                               text=True,
                                               suffix=".json")
    os.write(input_fd, manifest_file_contents)
    os.close(input_fd)

    original_file_name = 'original_file.ts'
    input_file_name = (_HERE_DIR / 'generated_file_pre_merge.js').resolve()
    output_file_name = (Path(self._out_folder) / "merged_maps.out").resolve()
    response_file_name = self._writeResponseFileContents(
        input_file_name, output_file_name, manifest_file)
    node.RunNode([
        str(_SOURCE_MAP_MERGER),
        '--response-file-name',
        str(response_file_name),
        '--out-dir',
        'tsc',
    ])

    manifest_file_contents = '{"base_dir":"tsc"}'
    manifest_file_path = Path(manifest_file)
    remapped_manifest_file = manifest_file_path.parent.joinpath(
        str(manifest_file_path.stem) + '__processed' +
        str(manifest_file_path.suffix))
    self.assertTrue(remapped_manifest_file.exists())
    with remapped_manifest_file.open(encoding='utf-8') as f:
      self.assertEqual(f.read(), manifest_file_contents)


if __name__ == '__main__':
  unittest.main()
