#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper around clang for nocompile tests.

The actual test functionality is largely implemented by clang itself. The
wrapper script exists for two purposes:
- generating an empty object file, so that nocompile GN targets can masquerade
  as source sets.
- generating a depfile on Windows. Normally, ninja parses /showIncludes output;
  unfortunately, this only works for compiler tools, not customm GN actions.
"""

import argparse
import pathlib
import os
import subprocess
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'build'))
import action_helpers


def main():
  parser = argparse.ArgumentParser(prog=sys.argv[0])
  parser.add_argument('--generate-depfile', action='store_true')
  parser.add_argument('compiler')
  parser.add_argument('source_path')
  parser.add_argument('obj_path')
  parser.add_argument('depfile_path')
  parser.add_argument('compiler_options', nargs=argparse.REMAINDER)

  args = parser.parse_args()

  compiler_args = [
      args.compiler,
  ]
  compiler_args += args.compiler_options
  compiler_args += [
      '-c',
      args.source_path,
  ]

  result = subprocess.run(compiler_args, stdout=subprocess.PIPE)

  if result.returncode == 0:
    pathlib.Path(args.obj_path).touch()
    if args.generate_depfile:
      # /showIncludes format:
      # Note: including file: third_party/libc++/src/include/stdio.h
      # Note: including file:  third_party/libc++/src/include/__config
      # Note: including file:   buildtools/third_party/libc++/__config_site
      # Note: including file: third_party/libc++/src/include/stdint.h

      # The prefix is locale-sensitive, but in practice, everything in the
      # Chrome build assumes the prefix is fixed.
      INCLUDE_PREFIX = 'Note: including file: '
      includes = map(
          lambda x: os.path.relpath(x[len(INCLUDE_PREFIX):].strip()),
          filter(
              lambda x: x.startswith(INCLUDE_PREFIX),
              result.stdout.decode('utf-8').splitlines(),
          ),
      )

      action_helpers.write_depfile(args.depfile_path, args.obj_path, includes)
  sys.exit(result.returncode)


if __name__ == '__main__':
  main()
