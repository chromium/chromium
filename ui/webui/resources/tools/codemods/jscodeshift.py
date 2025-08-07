#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
"""
Helper script for running jscodeshift codemods over a set of files.
"""

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(
    os.path.join(_HERE_PATH, '..', '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
"""
 Instructions to run this script locally.
 1) Create a package.json file in the same folder with the following contents.

{
  "dependencies": {
    "jscodeshift": "17.3.0"
  }
}

 2) Run 'npm install' in the same folder.

 3) Invoke the script from the root directory of the repository. For example

    python3 ui/webui/resources/tools/codemods/jscodeshift.py \
        --transform ui/webui/resources/tools/codemods/my_transform.js
        --files ui/webui/resources/cr_elements/cr_button/cr_button.ts

    python3 ui/webui/resources/tools/codemods/jscodeshift.py \
        --transform ui/webui/resources/tools/codemods/my_transform.js
        --files `find ui/webui/resources/cr_elements/cr_button/ -name '*.ts'`
"""


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--transform', required=True)
  parser.add_argument('--files', nargs='*', required=True)
  args = parser.parse_args(argv)

  if not os.path.exists(args.transform):
    print(
        f'Error: jscodeshift.py: Could not file transform file \'args.transform\'',
        file=sys.stderr)
    sys.exit(1)

  print(f'Migrating {len(args.files)} files...')

  # Update TS file.
  node.RunNode([
      os.path.join(_HERE_PATH, 'node_modules/jscodeshift/bin/jscodeshift.js'),
      '--transform=' + args.transform,
      '--extensions=ts',
      '--parser=ts',
      '--no-babel',
      '--fail-on-error',
  ] + args.files)

  print('DONE')


if __name__ == '__main__':
  main(sys.argv[1:])
