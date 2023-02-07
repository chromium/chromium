#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
from pathlib import Path

_HERE_DIR = Path(__file__).parent
_SOURCE_MAP_CREATOR = (_HERE_DIR / 'create_js_source_maps.js').resolve()

_NODE_PATH = (_HERE_DIR.parent.parent.parent.parent / 'third_party' /
              'node').resolve()
sys.path.append(str(_NODE_PATH))
import node


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--originals', required=True, nargs="*")
  parser.add_argument('--sources', required=True, nargs="*")
  parser.add_argument('--outputs', required=True, nargs="*")
  parser.add_argument('--inline-sourcemaps', action='store_true')
  args = parser.parse_args(argv)

  # Invokes "node create_js_source_maps.js (args)""
  # We can't use third_party/node/node.py directly from the gni template
  # because we don't have a good way to specify the path to
  # create_js_source_maps.js in a gni template.
  node.RunNode([
      str(_SOURCE_MAP_CREATOR),
      "--originals",
      *args.originals,
      "--inputs",
      *args.sources,
      "--outputs",
      *args.outputs,
  ] + ['--inline-sourcemaps'] if args.inline_sourcemaps else [])


if __name__ == '__main__':
  main(sys.argv[1:])
