#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys

_HERE_DIR = pathlib.Path(os.path.dirname(__file__)).resolve()
_SRC_DIR = _HERE_DIR.parents[3]

sys.path.append(str(_SRC_DIR / 'third_party' / 'node'))
import node
import node_modules


def main():
  try:
    node.RunNode([
        node_modules.PathToTypescript(), '--project',
        str(_HERE_DIR / 'jsconfig.json')
    ])
  except RuntimeError as e:
    # Skip first line, which is just error text added by node.RunNode().
    lines = str(e).splitlines()[1:]
    for line in lines:
      print(line)
    sys.exit(1)


if __name__ == '__main__':
  main()
