#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import os
import re
import shlex
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
tool_dir = os.path.abspath(os.path.join(script_dir, '../pylib'))
sys.path.insert(0, tool_dir)

from clang import compile_db

_PROBABLY_CLANG_RE = re.compile(r'clang(?:\+\+)?$')


def ParseArgs():
  parser = argparse.ArgumentParser(
      description='Utility to build one Chromium file for debugging clang')
  parser.add_argument('-p', required=True, help='path to the compile database')
  parser.add_argument('--generate-compdb',
                      action='store_true',
                      help='regenerate the compile database')
  parser.add_argument('--prefix',
                      help='optional prefix to prepend, e.g. --prefix=lldb')
  parser.add_argument(
      '--compiler',
      help='compiler to override the compiler specied in the compile db')
  parser.add_argument('--suffix',
                      help='optional suffix to append, e.g.' +
                      ' --suffix="-Xclang -ast-dump -fsyntax-only"')
  parser.add_argument('target_file', help='file to build')
  return parser.parse_args()


def BuildIt(record, prefix, compiler, suffix):
  """Builds the file in the provided compile DB record.

  Args:
    prefix: Optional prefix to prepend to the build command.
    compiler: Optional compiler to override the compiler specified the record.
    suffix: Optional suffix to append to the build command.
  """
  raw_args = shlex.split(record['command'])
  # The compile command might have some goop in front of it, e.g. if the build
  # is using goma, so shift arguments off the front until raw_args[0] looks like
  # a clang invocation.
  while raw_args:
    if _PROBABLY_CLANG_RE.search(raw_args[0]):
      break
    raw_args = raw_args[1:]
  if not raw_args:
    print('error: command %s does not appear to invoke clang!' %
          record['command'])
    return 2
  args = []
  if prefix:
    args.extend(shlex.split(prefix))
  if compiler:
    raw_args[0] = compiler
  args.extend(raw_args)
  if suffix:
    args.extend(shlex.split(suffix))
  print('Running %s' % ' '.join(args))
  os.execv(args[0], args)


def main():
  args = ParseArgs()
  os.chdir(args.p)
  if args.generate_compdb:
    with open('compile_commands.json', 'w') as f:
      f.write(compile_db.GenerateWithNinja('.'))
  db = compile_db.Read('.')
  for record in db:
    if os.path.normpath(os.path.join(args.p, record[
        'file'])) == args.target_file:
      return BuildIt(record, args.prefix, args.compiler, args.suffix)
  print('error: could not find %s in compile DB!' % args.target_file)
  return 1


if __name__ == '__main__':
  sys.exit(main())
