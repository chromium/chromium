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

from parameterized import parameterized
from pathlib import Path

_HERE_DIR = Path(__file__).parent.resolve()
_SOURCE_MAP_PROCESSOR = (_HERE_DIR.parent /
                         'create_js_source_maps.js').resolve()
_SOURCE_MAP_TRANSLATOR = (_HERE_DIR / 'translate_source_map.js').resolve()
_SOURCE_MAP_PREFIX = b'//# sourceMappingURL=data:application/json;base64,'

_NODE_PATH = (_HERE_DIR.parent.parent.parent.parent.parent / 'third_party' /
              'node').resolve()
sys.path.append(str(_NODE_PATH))
import node


class CreateSourceMapsTest(unittest.TestCase):
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
    assert isinstance(result['line'], int)
    assert isinstance(result['column'], int)
    return result['line'], result['column']

  @parameterized.expand([(True, ), (False, )])
  def testPostProcessedFile(self, inline_sourcemap):
    ''' Test that a known starting file translates back correctly

    Assume we start with the following file:
    Line 1
    // <if expr="foo"> Line 2
    Line 3 deleted
    // Line 4 </if>
    Line 5
    // <if expr="bar"> Line 6
    Line 7 deleted
    Line 8 deleted
    // Line 9 </if>
    Line 10
    Line 11

    Make sure we can map the various non-deleted lines back to their correct
    locations.
    '''
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)

    file_after_preprocess = b'''Line 1
      // /*grit-removed-lines:2*/
      Line 5
      // /*grit-removed-lines:3*/
      Line 10
      Line 11
      '''
    input_fd, input_file_name = tempfile.mkstemp(dir=self._out_folder,
                                                 text=True,
                                                 suffix=".js")
    os.write(input_fd, file_after_preprocess)
    os.close(input_fd)
    original_file_name = os.path.join(self._out_folder, "input.js")
    output_file_name = input_file_name + ".out"
    shutil.copyfile(os.path.join(_HERE_DIR, "input.js"), original_file_name)
    node.RunNode([
        str(_SOURCE_MAP_PROCESSOR),
        "--originals={}".format(" ".join([original_file_name])),
        "--inputs={}".format(" ".join([input_file_name])),
        "--outputs={}".format(" ".join([output_file_name])),
    ] + (["--inline-sourcemaps"] if inline_sourcemap else []))

    if inline_sourcemap:
      with open(output_file_name, 'rb') as output_file:
        # Strip the \r characters which are generated on Windows.
        output = output_file.read().replace(b'\r', b'')
      output_lines = output.splitlines()

      # Check source map was appended properly.
      self.assertGreaterEqual(len(output_lines), 1)
      self.assertEqual(file_after_preprocess,
                       output[:-(len(output_lines[-1]) + 1)])
      self.assertTrue(output_lines[-1].startswith(_SOURCE_MAP_PREFIX))

      source_map = base64.b64decode(output_lines[-1][len(_SOURCE_MAP_PREFIX):])
    else:
      with open(output_file_name) as map_file:
        source_map = map_file.read()

    json_sourcemap = json.loads(source_map)
    self.assertEqual(output_file_name, json_sourcemap['file'])
    self.assertEqual(os.getcwd(), json_sourcemap['sourceRoot'])
    self.assertEqual(original_file_name, json_sourcemap['sources'][0])

    # Check mappings:
    # Line 1 is before any removed lines, so it still maps to line 1
    line, column = self._translate(source_map, 1, 2)
    self.assertEqual(line, 1)
    # Column number always snaps back to the column number of the most recent
    # mapping point, so it's zero not the correct column number. This seems to
    # be a limitation of the sourcemap format.
    self.assertEqual(column, 0)

    # Original line 5 ends up on translated line 3
    line, column = self._translate(source_map, 3, 2)
    self.assertEqual(line, 5)
    self.assertEqual(column, 0)

    # Original line 10 ends up on line 5
    line, column = self._translate(source_map, 5, 2)
    self.assertEqual(line, 10)
    self.assertEqual(column, 0)

    # Original line 11 ends up on line 6
    line, column = self._translate(source_map, 6, 2)
    self.assertEqual(line, 11)
    self.assertEqual(column, 0)


if __name__ == '__main__':
  unittest.main()
