#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

_CWD = os.getcwd()


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_files', nargs='*', default=[])
  parser.add_argument('--dep_manifests', nargs='*', default=[])
  parser.add_argument('--base_dir')
  parser.add_argument('--dts_dir')
  parser.add_argument('--import_path_prefix', default='')
  parser.add_argument('--serve_path_prefix', default='')
  parser.add_argument('--out_manifest', required=True)
  args = parser.parse_args(argv)

  entries = []
  seen_imports = {}

  def add_entry(entry):
    entry_base_dir = entry['base_dir'].replace('\\', '/').rstrip('/')
    entry_dts_dir = (
      entry.get('dts_dir', entry_base_dir).replace('\\', '/').rstrip('/')
    )
    entry_import_prefix = (
      entry.get('import_path_prefix', '').replace('\\', '/').strip('/')
    )
    entry_serve_prefix = (
      entry.get('serve_path_prefix', '').replace('\\', '/').rstrip('/')
    )
    new_files = []
    for file_name in entry['files']:
      file_name = file_name.replace('\\', '/').lstrip('/')
      import_path = (
        f'{entry_import_prefix}/{file_name}'
        if entry_import_prefix
        else file_name
      )
      target_dirs = (
        entry_base_dir,
        entry_dts_dir,
        entry_serve_prefix,
        file_name,
      )
      if import_path in seen_imports:
        assert seen_imports[import_path] == target_dirs, (
          f'Conflicting manifest entries for {import_path}: '
          f'{seen_imports[import_path]} vs {target_dirs}'
        )
        continue
      seen_imports[import_path] = target_dirs
      new_files.append(file_name)

    if new_files:
      normalized = {
        'base_dir': entry_base_dir,
        'dts_dir': entry_dts_dir,
        'files': new_files,
      }
      if entry_import_prefix:
        normalized['import_path_prefix'] = entry_import_prefix
      if entry_serve_prefix:
        normalized['serve_path_prefix'] = entry_serve_prefix
      entries.append(normalized)

  for dep_manifest in args.dep_manifests:
    dep_path = os.path.normpath(os.path.join(_CWD, dep_manifest))
    with open(dep_path, 'r', encoding='utf-8') as f:
      data = json.load(f)
      # Support both `generate_mojo_manifest()` manifests (with an `entries`
      # list) and flat `base_dir` + `files` manifests.
      for dep_entry in data.get('entries', [data]):
        add_entry(dep_entry)

  if args.in_files:
    assert args.base_dir, '--base_dir is required when --in_files is specified.'
    dts_dir = args.dts_dir or args.base_dir
    add_entry(
      {
        'base_dir': args.base_dir,
        'dts_dir': dts_dir,
        'import_path_prefix': args.import_path_prefix,
        'serve_path_prefix': args.serve_path_prefix,
        'files': args.in_files,
      }
    )

  manifest_path = os.path.normpath(os.path.join(_CWD, args.out_manifest))
  with open(manifest_path, 'w', encoding='utf-8') as f:
    json.dump({'entries': entries}, f)


if __name__ == '__main__':
  main(sys.argv[1:])
