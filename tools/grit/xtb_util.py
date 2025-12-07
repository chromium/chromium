#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool for interacting with .xtb files.

Currently the only functionality in this tool is to parse and dump the given
.xtb file to the console (useful for debugging .xtb parsing).
"""

import sys

from grit import xtb_reader


def main():
  if len(sys.argv) != 2:
    print(f'Usage: {sys.argv[0]} <xtb_file>')
    sys.exit(1)

  with open(sys.argv[1], 'rb') as f:
    xtb_reader.Parse(f, print)


if __name__ == "__main__":
  main()
