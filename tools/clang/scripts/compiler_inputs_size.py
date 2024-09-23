#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script measures the compiler input size for a build target and each
translation unit therein. The input size of a translation unit is the sum size
of the source file and all files #included by it.

As input, the script takes the output of "ninja -t commands <target>" and "ninja
-t deps", which tells the script which source files are involved in the build
and what the dependencies of each file are.

In order to get accurate dependency information, the target must first have been
built. Also, the gn flag 'system_headers_in_deps' must be enabled to capture
dependencies on system headers.

Unlike analyze_includes.py, this script does not compute the full include graph,
which means it runs significantly faster in exchange for providing more limited
information. On a fast machine, it should take less than a minute for the
'chrome' target.

This currently doesnt work on Windows due to different deps handling.

Example usage: (Remove use_remoteexec=true if you don't have reclient access.)

$ gn gen out/Debug --args="system_headers_in_deps=true enable_nacl=false
      symbol_level=0 use_remoteexec=true"
$ autoninja -C out/Debug chrome
$ tools/clang/scripts/compiler_inputs_size.py out/Debug \
      <(ninja -C out/Debug -t commands chrome) <(ninja -C out/Debug -t deps)
apps/app_lifetime_monitor.cc 9,034,754
apps/app_lifetime_monitor_factory.cc 5,863,660
apps/app_restore_service.cc 9,198,130
[...]

Total: 233,482,538,553
"""

import argparse
import doctest
import os
import pathlib
import re
import sys

norm_paths = {}  # Memoization cache for norm_path().


def norm_path(build_dir, filename):
  if not filename in norm_paths:
    p = pathlib.Path(os.path.join(build_dir, filename)).resolve()
    x = os.path.relpath(p)
    norm_paths[filename] = x
  return norm_paths[filename]


file_sizes = {}  # Memoization cache for size().


def size(filename):
  """Get the size of a file."""
  if not filename in file_sizes:
    file_sizes[filename] = os.path.getsize(filename)
  return file_sizes[filename]


def parse_deps(build_dir, deps_output):
  r"""Parse the output of 'ninja -t deps', which contains information about
  which source files each object file depends on.

  Returns a dict of sets, e.g. {'foo.cc': {'foo.h', 'bar.h'}}.

  >>> deps = parse_deps(
  ...   'dir1/dir2',
  ...   'obj/foo.o: #deps 3, deps mtime 123456789 (VALID)\n'
  ...   '    ../../foo.cc\n'
  ...   '    ../../foo.h\n'
  ...   '    ../../bar.h\n'
  ...   '\n'
  ...   'obj/bar.o: #deps 3, deps mtime 123456789 (VALID)\n'
  ...   '    ../../bar.cc\n'
  ...   '    ../../bar.h\n'
  ...   '    gen.h\n'
  ...   '\n'.splitlines(keepends=True))
  >>> sorted(deps.keys())
  ['bar.cc', 'foo.cc']
  >>> sorted(deps['foo.cc'])
  ['bar.h', 'foo.h']
  >>> sorted(deps['bar.cc'])
  ['bar.h', 'dir1/dir2/gen.h']

  >>> deps = parse_deps(
  ...   'dir1/dir2',
  ...   'obj/foo.o: #deps 2, deps mtime 123456789 (STALE)\n'
  ...   '    ../../foo.cc\n'
  ...   '    ../../foo.h\n'
  ...   '\n'
  ...   'obj/bar.o: #deps 2, deps mtime 123456789 (VALID)\n'
  ...   '    ../../bar.cc\n'
  ...   '    ../../bar.h\n'
  ...   '\n'.splitlines(keepends=True))
  >>> sorted(deps.keys())
  ['bar.cc']

  >>> deps = parse_deps(
  ...   'dir1/dir2',
  ...   'obj/x86/foo.o: #deps 2, deps mtime 123456789 (VALID)\n'
  ...   '    ../../foo.cc\n'
  ...   '    ../../foo.h\n'
  ...   '\n'
  ...   'obj/arm/foo.o: #deps 2, deps mtime 123456789 (VALID)\n'
  ...   '    ../../foo.cc\n'
  ...   '    ../../foo_arm.h\n'
  ...   '\n'.splitlines(keepends=True))
  >>> sorted(deps['foo.cc'])
  ['foo.h', 'foo_arm.h']
  """

  # obj/foo.o: #deps 3, deps mtime 123456789 (VALID)
  #     ../../foo.cc
  #     ../../foo.h
  #     ../../bar.h
  #
  HEADER_RE = re.compile(r'.*: #deps (\d+), deps mtime \d+ \((VALID|STALE)\)')

  deps = dict()
  deps_iter = iter(deps_output)
  while True:
    # Read the deps header line.
    try:
      line = next(deps_iter)
    except StopIteration:
      break
    m = HEADER_RE.match(line)
    if not m:
      raise Exception("Unexpected deps header line: '%s'" % line)
    num_deps = int(m.group(1))
    if m.group(2) == 'STALE':
      # A deps entry is stale if the .o file doesn't exist or if it's newer than
      # the deps entry. Skip such entries.
      for _ in range(num_deps + 1):
        next(deps_iter)
      continue

    # Read the main file line.
    line = next(deps_iter)
    if not line.startswith('    '):
      raise Exception("Unexpected deps main file line '%s'" % line)
    main_file = norm_path(build_dir, line[4:].rstrip('\n'))
    deps.setdefault(main_file, set())

    # Read the deps lines.
    for _ in range(num_deps - 1):
      line = next(deps_iter)
      if not line.startswith('    '):
        raise Exception("Unexpected deps file line '%s'" % line)
      dep_file = norm_path(build_dir, line[4:].rstrip('\n'))
      deps[main_file].add(dep_file)

    # Read the blank line.
    line = next(deps_iter)
    if line != '\n':
      raise Exception("Expected a blank line but got '%s'" % line)

  return deps


def parse_commands(build_dir, commands_output):
  r"""Parse the output of 'ninja -t commands <target>' to extract which source
  files are involved in building that target. Returns a set, e.g. {'foo.cc',
  'bar.cc'}.

  >>> sorted(parse_commands('dir1/dir2',
  ...  '/x/rewrapper ../y/clang++ -a -b -c ../../foo.cc -o foo.o\n'
  ...  'clang -x blah -c ../../bar.c -o bar.o\n'
  ...  'clang-cl.exe /Fobaz.o /c baz.cc\n'.splitlines(keepends=True)))
  ['bar.c', 'dir1/dir2/baz.cc', 'foo.cc']
  """
  COMPILE_RE = re.compile(r'.*clang.* [/-]c (\S+)')
  files = set()
  for line in commands_output:
    m = COMPILE_RE.match(line)
    if m:
      files.add(norm_path(build_dir, m.group(1)))
  return files


def main():
  if doctest.testmod()[0]:
    return 1

  parser = argparse.ArgumentParser(description='Measure compiler input sizes '
                                   'for a build target.')
  parser.add_argument('build_dir', type=str, help='Chromium build dir')
  parser.add_argument('commands',
                      type=argparse.FileType('r', errors='ignore'),
                      help='File with the output of "ninja -t commands"')
  parser.add_argument('deps',
                      type=argparse.FileType('r', errors='ignore'),
                      help='File with the output of "ninja -t deps"')
  args = parser.parse_args()

  if sys.platform == 'win32':
    print('Not currently supported on Windows due to different deps handling.')
    return 1

  if not os.path.isdir(args.build_dir):
    print("Invalid build dir: '%s'" % args.build_dir)
    return 1

  deps = parse_deps(args.build_dir, args.deps)
  if not deps:
    print('Error: empty deps file.')
    return 1

  files = parse_commands(args.build_dir, args.commands)
  if not files:
    print('Error: empty commands file.')
    return 1

  total = 0
  for f in sorted(files):
    if f not in deps:
      raise Exception("Missing deps for '%s'", f)
    s = size(f) + sum(size(d) for d in deps[f])
    print('{} {}'.format(f, s))
    total += s

  print()
  print('Total: {}'.format(total))

  if not any(file.endswith('stddef.h') for file in file_sizes):
    print('Warning: did not see stddef.h.')
    print('Was the build configured with system_headers_in_deps=true?')

  return 0


if __name__ == '__main__':
  sys.exit(main())
