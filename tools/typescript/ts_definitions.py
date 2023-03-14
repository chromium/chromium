# Copyright 2021 The Chromium Authors
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


_TSCONFIG_BASE = 'tsconfig_definitions_base.json'
_TSCONFIG_GEN = 'tsconfig_definitions.json'


def _write_tsconfig_json(gen_dir, tsconfig):
  if not os.path.exists(gen_dir):
    os.makedirs(gen_dir)

  with open(os.path.join(gen_dir, _TSCONFIG_GEN), 'w',
            encoding='utf-8') as generated_tsconfig:
    json.dump(tsconfig, generated_tsconfig, indent=2)
  return


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--deps', nargs='*')
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--out_dir', required=True)
  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--js_files', nargs='*', required=True)
  parser.add_argument('--path_mappings', nargs='*')
  args = parser.parse_args(argv)

  with open(os.path.join(_HERE_DIR, _TSCONFIG_BASE),
            encoding='utf-8') as root_tsconfig:
    tsconfig = json.loads(root_tsconfig.read())

  root_dir = os.path.relpath(args.root_dir, args.gen_dir)
  out_dir = os.path.relpath(args.out_dir, args.gen_dir)

  tsconfig['files'] = [os.path.join(root_dir, f) for f in args.js_files]
  tsconfig['compilerOptions']['rootDir'] = root_dir
  tsconfig['compilerOptions']['outDir'] = out_dir
  if tsconfig['compilerOptions']['typeRoots'] is not None:
    tsconfig['compilerOptions']['typeRoots'] = \
        [os.path.relpath(os.path.join(_HERE_DIR, f), args.gen_dir) for f \
             in tsconfig['compilerOptions']['typeRoots']]

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

  if (args.root_dir == args.out_dir):
    # Delete .d.ts files if they already exist, otherwise TypeScript compiler
    # throws "error TS5055: Cannot write file ... because it would overwrite
    # input file" errors.
    for f in args.js_files:
      to_delete = os.path.join(args.out_dir, re.sub(r'\.js$', '.d.ts', f))
      if os.path.exists(to_delete):
        os.remove(to_delete)

  stdout = node.RunNode([
      node_modules.PathToTypescript(), '--project',
      os.path.join(args.gen_dir, _TSCONFIG_GEN)
  ])

  # Verify that that no unexpected .d.ts files were generated.
  lines = stdout.splitlines()
  token = 'TSFILE: '
  generated_files = []
  for l in lines:
    if token in l:
      generated_files.append(
          os.path.normpath(os.path.relpath(l[len(token):], args.out_dir)))

  args.js_files.sort()
  generated_files_set = set(generated_files)

  for i, _js_file in enumerate(args.js_files):
    js_file = os.path.normpath(_js_file)

    expected_file = base = os.path.splitext(js_file)[0] + '.d.ts'

    if expected_file in generated_files_set:
      # Remove the file from the set, to check at the end if any unexpected
      # files were generated.
      generated_files_set.remove(expected_file)

  unexpected_files_found = len(generated_files_set) > 0

  # Delete all generated files to not pollute the gen/ folder with any invalid
  # files, which could cause problems on subsequent builds.
  if unexpected_files_found:
    for f in generated_files:
      os.remove(os.path.join(args.out_dir, f))

    raise Exception(\
        'Unexpected file(s) \'%s\' generated, deleting all generated files.' \
        % generated_files_set)


if __name__ == '__main__':
  main(sys.argv[1:])
