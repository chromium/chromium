#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

_CWD = os.getcwd()


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--util_path', required=True)
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--in_files', nargs='*', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--out_manifest', required=True)

  args = parser.parse_args(argv)

  util_path = os.path.normpath(
      os.path.join(_CWD, args.util_path).replace('\\', '/'))
  in_folder = os.path.normpath(
      os.path.join(_CWD, args.in_folder).replace('\\', '/'))
  out_folder = os.path.normpath(
      os.path.join(_CWD, args.out_folder).replace('\\', '/'))
  out_manifest = os.path.normpath(
      os.path.join(_CWD, args.out_manifest).replace('\\', '/'))
  out_file_suffix = '.code_cache'

  subprocess.run([
      util_path, f'--in_folder={in_folder}',
      '--in_files=' + ','.join(args.in_files), f'--out_folder={out_folder}',
      f'--out_file_suffix={out_file_suffix}'
  ],
                 check=True)

  manifest_data = {
      'base_dir': args.out_folder,
      'files': [],
  }
  for in_file in args.in_files:
    manifest_data['files'].append(in_file + out_file_suffix)

  with open(out_manifest, 'w', newline='', encoding='utf-8') as manifest_file:
    json.dump(manifest_data, manifest_file)


if __name__ == '__main__':
  main(sys.argv[1:])
