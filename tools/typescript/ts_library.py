# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import json
import os
import re
import sys
import io

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


def _validate_tsconfig_json(tsconfig_file):
  with io.open(tsconfig_file, encoding='utf-8', mode='r') as f:
    tsconfig = json.loads(f.read())

    if 'compilerOptions' in tsconfig and \
        'composite' in tsconfig['compilerOptions']:
      return False, f'Invalid |composite| flag detected in {tsconfig_file}.' + \
          ' Use the dedicated |composite=true| attribute in ts_library() ' + \
          'instead.'
  return True, None


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--deps', nargs='*')
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--path_mappings', nargs='*')
  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--out_dir', required=True)
  parser.add_argument('--tsconfig_base')
  parser.add_argument('--in_files', nargs='*')
  parser.add_argument('--manifest_excludes', nargs='*')
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

  tsconfig_base_file = os.path.normpath(
      os.path.join(args.gen_dir, tsconfig['extends']))

  is_tsconfig_valid, error = _validate_tsconfig_json(tsconfig_base_file)
  if not is_tsconfig_valid:
    raise AssertionError(error)

  tsconfig['compilerOptions'] = collections.OrderedDict()
  tsconfig['compilerOptions']['rootDir'] = root_dir
  tsconfig['compilerOptions']['outDir'] = out_dir

  if args.composite:
    tsbuildinfo_name = 'tsconfig.tsbuildinfo'
    tsconfig['compilerOptions']['composite'] = True
    tsconfig['compilerOptions']['declaration'] = True
    tsconfig['compilerOptions']['tsBuildInfoFile'] = tsbuildinfo_name

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

  # Detect and delete obsolete files that can cause build problems.
  if args.in_files is not None:
    for f in args.in_files:
      [pathname, extension] = os.path.splitext(f)

      # Delete any obsolete .ts files (from previous builds) corresponding to
      # .js |in_files| in the |root_dir| folder, as they would cause the
      # following error to be thrown:
      #
      # "error TS5056: Cannot write file '...' because it would be overwritten
      # by multiple input files."
      #
      # This can happen when a ts_library() is migrating JS to TS one file at a
      # time and a bot is switched from building a later CL to building an
      # earlier CL.
      if extension == '.js':
        to_check = os.path.join(args.root_dir, pathname + '.ts')
        if os.path.exists(to_check):
          os.remove(to_check)

      # Delete any obsolete .d.ts files (from previous builds) corresponding to
      # .ts |in_files| in |root_dir| folder.
      #
      # This can happen when a ts_library() is migrating JS to TS one file at a
      # time and a previous checked-in or auto-generated .d.ts file is now
      # obsolete.
      if extension == '.ts':
        to_check = os.path.join(args.root_dir, pathname + '.d.ts')
        if os.path.exists(to_check):
          os.remove(to_check)

  try:
    node.RunNode([
        node_modules.PathToTypescript(), '--project',
        os.path.join(args.gen_dir, 'tsconfig.json')
    ])
  finally:
    if args.composite:
      # `.tsbuildinfo` is generated by TypeScript for incremenetal compilation
      # freshness checks. Since GN already decides which ts_library() targets
      # are dirty, `.tsbuildinfo` is not needed for our purposes and is
      # deleted.
      #
      # Moreover `.tsbuildinfo` can cause flakily failing builds since the TS
      # compiler checks the `.tsbuildinfo` file and sees that none of the
      # source files are changed and does not regenerate any output, without
      # checking whether output files have been modified/deleted, which can
      # lead to bad builds (missing files or picking up obsolete generated
      # files).
      tsbuildinfo_path = os.path.join(args.gen_dir, tsbuildinfo_name)
      if os.path.exists(tsbuildinfo_path):
        os.remove(tsbuildinfo_path)

  if args.in_files is not None:
    with open(os.path.join(args.gen_dir, 'tsconfig.manifest'), 'w') \
        as manifest_file:
      manifest_data = {}
      manifest_data['base_dir'] = args.out_dir
      manifest_files = args.in_files
      if args.manifest_excludes is not None:
        manifest_files = filter(lambda f: f not in args.manifest_excludes,
                                args.in_files)
      manifest_data['files'] = \
          [re.sub(r'\.ts$', '.js', f) for f in manifest_files]
      json.dump(manifest_data, manifest_file)


if __name__ == '__main__':
  main(sys.argv[1:])
