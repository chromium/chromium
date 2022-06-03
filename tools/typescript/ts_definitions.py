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


_TSCONFIG_BASE = 'tsconfig_definitions_base.json'
_TSCONFIG_GEN = 'tsconfig_definitions.json'


def _write_tsconfig_json(gen_dir, tsconfig):
  if not os.path.exists(gen_dir):
    os.makedirs(gen_dir)

  with open(os.path.join(gen_dir, _TSCONFIG_GEN), 'w') as generated_tsconfig:
    json.dump(tsconfig, generated_tsconfig, indent=2)
  return


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--out_dir', required=True)
  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--js_files', nargs='*', required=True)
  parser.add_argument('--path_mappings', nargs='*')
  args = parser.parse_args(argv)

  with open(os.path.join(_HERE_DIR, _TSCONFIG_BASE)) as root_tsconfig:
    tsconfig = json.loads(root_tsconfig.read())

  root_dir = os.path.relpath(args.root_dir, args.gen_dir)
  out_dir = os.path.relpath(args.out_dir, args.gen_dir)

  tsconfig['files'] = [os.path.join(root_dir, f) for f in args.js_files]
  tsconfig['compilerOptions']['rootDir'] = root_dir
  tsconfig['compilerOptions']['outDir'] = out_dir

  # Handle custom path mappings, for example chrome://resources/ URLs.
  if args.path_mappings is not None:
    path_mappings = collections.defaultdict(list)
    for m in args.path_mappings:
      mapping = m.split('|')
      path_mappings[mapping[0]].append(os.path.join('./', mapping[1]))
    tsconfig['compilerOptions']['paths'] = path_mappings

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

  generated_files.sort()
  args.js_files.sort()

  unexpected_file = None
  for i, _js_file in enumerate(args.js_files):
    js_file = os.path.normpath(_js_file)

    if os.path.dirname(js_file) != os.path.dirname(generated_files[i]):
      unexpected_file = generated_files[i]
      break

    base = os.path.splitext(os.path.basename(js_file))[0]
    if base + '.d.ts' != os.path.basename(generated_files[i]):
      unexpected_file = generated_files[i]
      break

  # Delete all generated files to not pollute the gen/ folder with any invalid
  # files, which could cause problems on subsequent builds.
  if unexpected_file is not None:
    for f in generated_files:
      os.remove(os.path.join(args.out_dir, f))

    raise Exception(\
        'Unexpected file \'%s\' generated, deleting all generated files.' \
        % unexpected_file)


if __name__ == '__main__':
  main(sys.argv[1:])
