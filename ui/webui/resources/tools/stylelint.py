#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))

_NODE_PATH = os.path.join(_SRC_PATH, 'third_party', 'node')
sys.path.append(_NODE_PATH)
import node
import node_modules


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--config', required=True)
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--in_files', nargs='*', required=True)
  parser.add_argument('--out_file', required=True)

  args = parser.parse_args(argv)

  if len(args.in_files) == 0:
    return

  in_files = list(map(lambda f: os.path.join(args.in_folder, f), args.in_files))

  chunk_size = len(in_files)
  if sys.platform == 'win32' and len(' '.join(in_files)) > 30000:
    # On Windows the maximum command line length for Python's CreateProcess()
    # function is 32767 characters. When the limit is exceeded a confusing
    # "FileNotFoundError: [WinError 206] The filename or extension is too long"
    # error is thrown. Split to multiple invocations to work around that.
    chunk_size = min(round(len(in_files) / 2), 100)

  in_files_chunks = [
      in_files[i:i + chunk_size] for i in range(0, len(in_files), chunk_size)
  ]

  for files in in_files_chunks:
    node.RunNode([
        node_modules.PathToStylelint(),
        # Force colored output, otherwise no colors appear when running locally.
        '--color',
        '--config',
        args.config,
    ] + files)

  with open(args.out_file, 'w', newline='', encoding='utf-8') as f:
    f.write('OK')


if __name__ == '__main__':
  main(sys.argv[1:])
