# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
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
  parser.add_argument('--out_dir', required=True)
  parser.add_argument('--tsconfig_base')
  parser.add_argument('--in_files', nargs='*')
  parser.add_argument('--definitions', nargs='*')
  parser.add_argument('--composite', action='store_true')
  args = parser.parse_args(argv)

  root_dir = os.path.relpath(args.root_dir, args.gen_dir)
  out_dir = os.path.relpath(args.out_dir, args.gen_dir)
  TSCONFIG_BASE_PATH = os.path.join(_HERE_DIR, 'tsconfig_base.json')

  tsconfig = collections.OrderedDict()

  tsconfig['extends'] = args.tsconfig_base \
      if args.tsconfig_base is not None \
      else os.path.relpath(TSCONFIG_BASE_PATH, args.gen_dir)

  tsconfig['compilerOptions'] = collections.OrderedDict()
  tsconfig['compilerOptions']['tsBuildInfoFile'] = 'tsconfig.tsbuildinfo'
  tsconfig['compilerOptions']['rootDir'] = root_dir
  tsconfig['compilerOptions']['outDir'] = out_dir

  if args.composite:
    tsconfig['compilerOptions']['composite'] = True
    tsconfig['compilerOptions']['declaration'] = True

  tsconfig['files'] = []
  if args.in_files is not None:
    # Source .ts files are always resolved as being relative to |root_dir|.
    tsconfig['files'].extend([os.path.join(root_dir, f) for f in args.in_files])

  if args.definitions is not None:
    tsconfig['files'].extend(args.definitions)

  # Handle custom path mappings, for example chrome://resources/ URLs.
  if args.path_mappings is not None:
    path_mappings = collections.defaultdict(list)
    for m in args.path_mappings:
      mapping = m.split('|')
      path_mappings[mapping[0]].append(os.path.join('./', mapping[1]))
    tsconfig['compilerOptions']['paths'] = path_mappings

  if args.deps is not None:
    tsconfig['references'] = [{'path': dep} for dep in args.deps]

  _write_tsconfig_json(args.gen_dir, tsconfig)

  node.RunNode([
      node_modules.PathToTypescript(), '--project',
      os.path.join(args.gen_dir, 'tsconfig.json')
  ])

  if args.in_files is not None:
    with open(os.path.join(args.gen_dir, 'tsconfig.manifest'), 'w') \
        as manifest_file:
      manifest_data = {}
      manifest_data['base_dir'] = args.out_dir
      manifest_data['files'] = \
          [re.sub(r'\.ts$', '.js', f) for f in args.in_files]
      json.dump(manifest_data, manifest_file)


if __name__ == '__main__':
  main(sys.argv[1:])
