#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Helper for generating compile DBs for clang tooling. On non-Windows platforms,
this is pretty straightforward. On Windows, the tool does a bit of extra work to
integrate the content of response files, force clang tooling to run in clang-cl
mode, etc.
"""

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
      '-p',
      required=True,
      help='Path to build directory')
  parser.add_argument(
      'targets',
      nargs='*',
      help='Additional targets to pass to ninja')
  parser.add_argument(
      '-o',
      help='File to write the compilation database to. Defaults to stdout')

  args = parser.parse_args()

  compdb_text = json.dumps(
      compile_db.ProcessCompileDatabaseIfNeeded(
          compile_db.GenerateWithNinja(args.p, args.targets)),
      indent=2)
  if args.o is None:
    print(compdb_text)
  else:
    with open(args.o, 'w') as f:
      f.write(compdb_text)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
