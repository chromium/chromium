#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
"""
Helper script for updating Polymer code to address https://crbug.com/389737066.
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
    "jscodeshift": "^0.15.1"
  }
}

 2) Run 'npm install' in the same folder.

 3) Invoke the script from the root directory of the repository. For example

    python3 ui/webui/resources/tools/codemods/389737066_migration.py \
        --files chrome/browser/resources/print_preview/ui/color_settings.js

    python3 ui/webui/resources/tools/codemods/389737066_migration.py \
        --files `find chrome/browser/resources/print_preview/ui/ -name '*.ts'`
"""


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--files', nargs='*', required=True)
  args = parser.parse_args(argv)

  print(f'Migrating {len(args.files)} files...')

  # Update TS file.
  out = node.RunNode([
      os.path.join(_HERE_PATH, 'node_modules/jscodeshift/bin/jscodeshift.js'),
      '--transform=' + os.path.join(_HERE_PATH, '389737066_migration.js'),
      '--extensions=ts',
      '--parser=ts',
  ] + args.files)

  print('DONE')


if __name__ == '__main__':
  main(sys.argv[1:])
