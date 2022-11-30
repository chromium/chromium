#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates compile DBs that are more amenable for clang tooling."""

import argparse
import json
import os
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
tool_dir = os.path.abspath(os.path.join(script_dir, '../pylib'))
sys.path.insert(0, tool_dir)

from clang import compile_db


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--filter_arg',
      nargs='*',
      help='Additional argument(s) to filter out from compilation database.')
  parser.add_argument(
      '-o',
      help='File to write the compilation database to. Defaults to stdout')
  parser.add_argument('-p', required=True, help='Path to build directory')
  parser.add_argument(
      '--target_os',
      choices=[
          'android',
          'chromeos',
          'fuchsia',
          'ios',
          'linux',
          'mac',
          'nacl',
          'win',
      ],
      help='Target OS - see `gn help target_os`. Set to "win" when ' +
      'cross-compiling Windows from Linux or another host')
  parser.add_argument('targets',
                      nargs='*',
                      help='Additional targets to pass to ninja')

  args = parser.parse_args()

  compdb_text = json.dumps(compile_db.ProcessCompileDatabase(
      compile_db.GenerateWithNinja(args.p, args.targets), args.filter_arg,
      args.target_os),
                           indent=2)
  if args.o is None:
    print(compdb_text)
  else:
    with open(args.o, 'w') as f:
      f.write(compdb_text)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
