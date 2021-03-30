# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys

_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)
_SRC_DIR = os.path.normpath(os.path.join(_HERE_DIR, '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'node'))
import node
import node_modules


def _write_tsconfig_json(gen_dir, tsconfig):
  if not os.path.exists(gen_dir):
    os.makedirs(gen_dir)

  with open(os.path.join(gen_dir, 'tsconfig.json'), 'w') as generated_tsconfig:
    json.dump(tsconfig, generated_tsconfig, indent=2)
  return


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--deps', nargs='*')
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--path_mappings', nargs='*')
  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--sources', nargs='*', required=True)
  parser.add_argument('--definitions', nargs='*')
  args = parser.parse_args(argv)

  root_dir = os.path.relpath(args.root_dir, args.gen_dir)
  sources = [os.path.join(root_dir, f) for f in args.sources]

  with open(os.path.join(_HERE_DIR, 'tsconfig_base.json')) as root_tsconfig:
    tsconfig = json.loads(root_tsconfig.read())

  tsconfig['files'] = sources
  if args.definitions is not None:
    # Definitions .d.ts files are always assumed to reside in |gen_dir|.
    tsconfig['files'].extend(args.definitions)

  tsconfig['compilerOptions']['rootDir'] = root_dir

  # Handle custom path mappings, for example chrome://resources/ URLs.
  if args.path_mappings is not None:
    path_mappings = {}
    for m in args.path_mappings:
      mapping = m.split('|')
      path_mappings[mapping[0]] = [os.path.join('./', mapping[1])]
    tsconfig['compilerOptions']['paths'] = path_mappings

  if args.deps is not None:
    tsconfig['references'] = [{'path': dep} for dep in args.deps]

  _write_tsconfig_json(args.gen_dir, tsconfig)

  node.RunNode([
      node_modules.PathToTypescript(), '--project',
      os.path.join(args.gen_dir, 'tsconfig.json')
  ])

  with open(os.path.join(args.gen_dir, 'tsconfig.manifest'), 'w') \
      as manifest_file:
    manifest_data = {}
    manifest_data['base_dir'] = args.gen_dir
    manifest_data['files'] = [re.sub(r'\.ts$', '.js', f) for f in args.sources]
    json.dump(manifest_data, manifest_file)


if __name__ == '__main__':
  main(sys.argv[1:])
