#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--out_folder', required=True)
  args = parser.parse_args(argv)

  node.RunNode([node_modules.PathToTerser(),
          os.path.join(_HERE_PATH, 'lottie_worker.js'),
          '--ascii_only=true',
          '-b', 'beautify=false',
          '--compress',
          '--mangle', 'reserved=[\'$\',\'onmessage\',\'postMessage\']',
          '--output', os.path.join(args.out_folder, 'lottie_worker.min.js')])

if __name__ == '__main__':
  main(sys.argv[1:])
