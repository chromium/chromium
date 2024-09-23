# Copyright 2021 The Chromium Authors
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

from path_utils import isInAshFolder, getTargetPath
from validate_tsconfig import validateTsconfigJson, validateJavaScriptAllowed, validateRootDir, isUnsupportedJsTarget, isMappingAllowed, validateDefinitionDeps


def _write_tsconfig_json(gen_dir, tsconfig, tsconfig_file):
  if not os.path.exists(gen_dir):
    os.makedirs(gen_dir)

  with open(os.path.join(gen_dir, tsconfig_file), 'w',
            encoding='utf-8') as generated_tsconfig:
    json.dump(tsconfig, generated_tsconfig, indent=2)
  return

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--deps', nargs='*')
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--path_mappings', nargs='*')
  parser.add_argument('--path_mappings_file')

  parser.add_argument('--root_gen_dir', required=True)
  parser.add_argument('--root_src_dir', required=True)

  parser.add_argument('--root_dir', required=True)
  parser.add_argument('--out_dir', required=True)
  parser.add_argument('--tsconfig_base')
  parser.add_argument('--in_files', nargs='*')
  parser.add_argument('--manifest_excludes', nargs='*')
  parser.add_argument('--definitions', nargs='*')
  parser.add_argument('--composite', action='store_true')
  parser.add_argument('--platform',
                      choices=['other', 'ios', 'chromeos_ash'],
                      default='other')
  parser.add_argument('--enable_source_maps', action='store_true')
  parser.add_argument('--output_suffix', required=True)
  args = parser.parse_args(argv)

  root_dir = os.path.relpath(args.root_dir, args.gen_dir)
  out_dir = os.path.relpath(args.out_dir, args.gen_dir)

  is_root_dir_valid, error = validateRootDir(args.root_dir, args.gen_dir,
                                             args.root_gen_dir,
                                             args.platform == 'ios')
  if not is_root_dir_valid:
    raise AssertionError(error)

  TSCONFIG_BASE_PATH = os.path.join(_HERE_DIR, 'tsconfig_base.json')

  tsconfig = collections.OrderedDict()

  tsconfig['extends'] = args.tsconfig_base \
      if args.tsconfig_base is not None \
      else os.path.relpath(TSCONFIG_BASE_PATH, args.gen_dir)

  tsconfig_base_file = os.path.normpath(
      os.path.join(args.gen_dir, tsconfig['extends']))

  tsconfig['compilerOptions'] = collections.OrderedDict()

  with io.open(tsconfig_base_file, encoding='utf-8', mode='r') as f:
    tsconfig_base = json.loads(f.read())

    is_base_tsconfig = args.tsconfig_base is None or \
        args.tsconfig_base.endswith('/tools/typescript/tsconfig_base.json')
    is_tsconfig_valid, error = validateTsconfigJson(tsconfig_base,
                                                    tsconfig_base_file,
                                                    is_base_tsconfig)
    if not is_tsconfig_valid:
      raise AssertionError(error)

    # Work-around for https://github.com/microsoft/TypeScript/issues/30024. Need
    # to append 'trusted-types' in cases where the default configuration's
    # 'types' field is overridden, because of the Chromium patch  at
    # third_party/node/typescript.patch
    # TODO(dpapad): Remove if/when the TypeScript bug has been fixed.
    if 'compilerOptions' in tsconfig_base and \
        'types' in tsconfig_base['compilerOptions']:
      types = tsconfig_base['compilerOptions']['types']

      if 'trusted-types' not in types:
        # Ensure that typeRoots is not overridden in an incompatible way.
        ERROR_MSG = ('Need to include \'third_party/node/node_modules/@types\' '
                     'when overriding the default typeRoots')
        assert ('typeRoots' in tsconfig_base['compilerOptions']), ERROR_MSG
        type_roots = tsconfig_base['compilerOptions']['typeRoots']
        has_type_root = any(r.endswith('third_party/node/node_modules/@types') \
            for r in type_roots)
        assert has_type_root, ERROR_MSG

        augmented_types = types.copy()
        augmented_types.append('trusted-types')
        tsconfig['compilerOptions']['types'] = augmented_types

  tsconfig['compilerOptions']['rootDir'] = root_dir
  tsconfig['compilerOptions']['outDir'] = out_dir

  includes_js = False
  if (args.in_files):
    for file in args.in_files:
      if file.endswith('.js'):
        includes_js = True

  if includes_js or isUnsupportedJsTarget(args.gen_dir, args.root_gen_dir):
    source_dir = os.path.realpath(os.path.join(_CWD, args.gen_dir,
                                               root_dir)).replace('\\', '/')
    out_dir = os.path.realpath(os.path.join(_CWD, args.gen_dir,
                                            out_dir)).replace('\\', '/')
    is_js_allowed, error = validateJavaScriptAllowed(source_dir, out_dir,
                                                     args.platform == 'ios')
    if not is_js_allowed:
      raise AssertionError(error)
    tsconfig['compilerOptions']['allowJs'] = True

  if args.composite:
    tsbuildinfo_name = f'tsconfig_{args.output_suffix}.tsbuildinfo'
    tsconfig['compilerOptions']['composite'] = True
    tsconfig['compilerOptions']['declaration'] = True
    tsconfig['compilerOptions']['tsBuildInfoFile'] = tsbuildinfo_name

  if args.enable_source_maps:
    tsconfig['compilerOptions']['inlineSourceMap'] = True
    tsconfig['compilerOptions']['inlineSources'] = True
    tsconfig['compilerOptions']['sourceRoot'] = os.path.realpath(
        os.path.join(_CWD, args.gen_dir, root_dir))

  tsconfig['files'] = []
  if args.in_files is not None:
    # Source .ts files are always resolved as being relative to |root_dir|.
    tsconfig['files'].extend([os.path.join(root_dir, f) for f in args.in_files])

  if args.definitions is not None:
    for d in args.definitions:
      assert d.endswith(
          '.d.ts'), f'Invalid definition \'{d}\'. Should end with \'.d.ts\''
    tsconfig['files'].extend(args.definitions)

  target_path = getTargetPath(args.gen_dir, args.root_gen_dir)
  is_ash_target = isInAshFolder(target_path)

  if args.deps is not None:
    tsconfig['references'] = [{'path': dep} for dep in args.deps]

  path_mappings = collections.defaultdict(list)
  # Load all mappings from the input file, if one exists.
  if (args.path_mappings_file is not None):
    path_mappings_path = os.path.join(args.gen_dir, args.path_mappings_file)
    with open(path_mappings_path, 'r', encoding='utf-8') as f:
      file_mappings = json.loads(f.read())
      for url in file_mappings:
        path_mappings[url] = file_mappings[url]

  # Add target-specified mappings.
  if args.path_mappings is not None:
    for m in args.path_mappings:
      mapping = m.split('|')
      mapping_path = os.path.relpath(mapping[1], args.root_src_dir)
      assert isMappingAllowed(is_ash_target, target_path, mapping_path), \
          f'Cannot use mapping to Ash-specific folder {mapping_path} from ' \
          f'non-Ash target {target_path}'
      path_mappings[mapping[0]].append(os.path.join('./', mapping[1]))

  tsconfig['compilerOptions']['paths'] = path_mappings

  tsconfig_file = f'tsconfig_{args.output_suffix}.json'
  _write_tsconfig_json(args.gen_dir, tsconfig, tsconfig_file)

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

        # Delete any obsolete .ts files (from previous builds) corresponding to
        # .ts |in_files| in |out_dir| folder, only done when |root_dir| and
        # |out_dir| are different folders.
        if args.root_dir != args.out_dir:
          to_check = os.path.join(args.out_dir, f)
          if os.path.exists(to_check):
            os.remove(to_check)

  try:
    node.RunNode([
        node_modules.PathToTypescript(), '--project',
        os.path.join(args.gen_dir, tsconfig_file)
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

  # Invoke the TS compiler again, with the --listFilesOnly flag, to detect any
  # files that are used by the build, but not properly declared as dependencies.
  out = node.RunNode([
      node_modules.PathToTypescript(),
      '--project',
      os.path.join(args.gen_dir, tsconfig_file),
      '--listFilesOnly',
  ])
  files_list = out.split('\n')
  definitions_files = list(filter(lambda f: f.endswith('.d.ts'), files_list))
  definitions = args.definitions if args.definitions is not None else []
  list_valid, error_msg = validateDefinitionDeps(definitions_files, target_path,
                                                 args.gen_dir,
                                                 args.root_gen_dir, definitions)
  if not list_valid:
    raise AssertionError(error_msg)

  if args.in_files is not None:
    manifest_path = os.path.join(args.gen_dir,
                                 f'{args.output_suffix}_manifest.json')
    with open(manifest_path, 'w', encoding='utf-8') as manifest_file:
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
