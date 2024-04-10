#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

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

    python3 ui/webui/resources/tools/codemods/lit_migration.py \
        --file ui/webui/resources/cr_components/most_visited/most_visited.ts
"""


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--file', required=True)
  args = parser.parse_args(argv)

  print(f'Migrating {args.file}...')

  # Update TS file.
  out = node.RunNode([
      os.path.join(_HERE_PATH, 'node_modules/jscodeshift/bin/jscodeshift.js'),
      '--transform=' + os.path.join(_HERE_PATH, 'lit_migration.js'),
      '--extensions=ts', '--parser=ts', args.file
  ])

  # Update HTML/CSS file.
  out = node.RunNode([
      os.path.join(_HERE_PATH, 'lit_migration_templates.mjs'),
      '--file=' + args.file,
  ])

  print('DONE')


if __name__ == '__main__':
  main(sys.argv[1:])
