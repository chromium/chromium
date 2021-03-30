# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys

_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)
_SRC_DIR = os.path.normpath(os.path.join(_HERE_DIR, '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'node'))
import node
import node_modules


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--js_files', nargs='*', required=True)
  args = parser.parse_args(argv)

  js_files = [os.path.join(args.root_dir, f) for f in args.js_files]

  node.RunNode([
      node_modules.PathToTypescript(),
      '--declaration',
      '--allowJs',
      '--emitDeclarationOnly',
      '--removeComments',
      '--noResolve',
      '--rootDir',
      args.root_dir,
      '--outDir',
      args.gen_dir,
  ] + js_files)


if __name__ == '__main__':
  main(sys.argv[1:])
