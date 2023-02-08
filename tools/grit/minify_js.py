#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..'))
sys.path.insert(0, os.path.join(_SRC_PATH, 'third_party', 'node'))

import node
import node_modules


def Minify(source):
  # Open two temporary files, so that terser can read the input from one and
  # write its output to the other.
  with tempfile.NamedTemporaryFile(mode="w+", suffix='.js') as infile, \
       tempfile.NamedTemporaryFile(mode="r+", suffix='.js') as outfile:
    infile.write(source)
    infile.flush()
    node.RunNode(
        [node_modules.PathToTerser(), infile.name, '--output', outfile.name])
    result = outfile.read()
    return result


def main():
  orig_stdout = sys.stdout
  result = ''
  try:
    sys.stdout = sys.stderr
    result = Minify(sys.stdin.read())
  finally:
    sys.stdout = orig_stdout
    print(result)


if __name__ == '__main__':
  main()
