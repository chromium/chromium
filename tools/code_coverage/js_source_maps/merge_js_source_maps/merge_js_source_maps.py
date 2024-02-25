#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
from pathlib import Path

_HERE_DIR = Path(__file__).parent
_SOURCE_MAP_MERGER = (_HERE_DIR / 'merge_js_source_maps.js').resolve()

_NODE_PATH = (_HERE_DIR.parent.parent.parent.parent / 'third_party' /
              'node').resolve()
sys.path.append(str(_NODE_PATH))
import node


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--out-dir', required=True)
  parser.add_argument('--response-file-name', required=True, nargs="*")
  args = parser.parse_args(argv)

  node.RunNode([
      str(_SOURCE_MAP_MERGER),
      '--response-file-name',
      *args.response_file_name,
      '--out-dir',
      args.out_dir,
  ])


if __name__ == '__main__':
  main(sys.argv[1:])
