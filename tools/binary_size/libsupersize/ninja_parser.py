#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract source file information from .ninja files."""

from __future__ import print_function

import argparse
import logging
import os
import re


# E.g.:
# build obj/.../foo.o: cxx gen/.../foo.cc || obj/.../foo.inputdeps.stamp
# build obj/.../libfoo.a: alink obj/.../a.o obj/.../b.o |
# build ./libchrome.so ./lib.unstripped/libchrome.so: solink a.o b.o ...
# build libmonochrome.so: __chrome_android_libmonochrome___rule | ...
_REGEX = re.compile(r'build ([^:]+): \w+ (.*?)(?: *\||\n|$)')


class _SourceMapper(object):
  def __init__(self, dep_map, parsed_file_count):
    self._dep_map = dep_map
    self.parsed_file_count = parsed_file_count
    self._unmatched_paths = set()

  def _FindSourceForPathInternal(self, path):
    if not path.endswith(')'):
      return self._dep_map.get(path)

    # foo/bar.a(baz.o)
    start_idx = path.index('(')
    lib_name = path[:start_idx]
    obj_name = path[start_idx + 1:-1]
    by_basename = self._dep_map.get(lib_name)
    if not by_basename:
      return None
    obj_path = by_basename.get(obj_name)
    if not obj_path:
      # Found the library, but it doesn't list the .o file.
      logging.warning('no obj basename for %s', path)
      return None
    return self._dep_map.get(obj_path)

  def FindSourceForPath(self, path):
    """Returns the source path for the given object path (or None if not found).

    Paths for objects within archives should be in the format: foo/bar.a(baz.o)
    """
    ret = self._FindSourceForPathInternal(path)
    if not ret and path not in self._unmatched_paths:
      if self.unmatched_paths_count < 10:
        logging.warning('Could not find source path for %s', path)
      self._unmatched_paths.add(path)
    return ret

  @property
  def unmatched_paths_count(self):
    return len(self._unmatched_paths)

  def IterAllPaths(self):
    return self._dep_map.iterkeys()


def _ParseNinjaPathList(path_list):
  ret = path_list.replace('\\ ', '\b')
  return [s.replace('\b', ' ') for s in ret.split()]


def _ParseOneFile(lines, dep_map, elf_path):
  sub_ninjas = []
  elf_inputs = None
  for line in lines:
    if line.startswith('subninja '):
      sub_ninjas.append(line[9:-1])
      continue
    m = _REGEX.match(line)
    if m:
      outputs, srcs = m.groups()
      if len(outputs) > 2 and outputs[-2] == '.' and outputs[-1] in 'ao':
        output = outputs.replace('\\ ', ' ')
        assert output not in dep_map, 'Duplicate output: ' + output
        if output[-1] == 'o':
          dep_map[output] = srcs.replace('\\ ', ' ')
        else:
          obj_paths = _ParseNinjaPathList(srcs)
          dep_map[output] = {os.path.basename(p): p for p in obj_paths}
      elif elf_path and elf_path in outputs:
        properly_parsed = [
            os.path.normpath(p) for p in _ParseNinjaPathList(outputs)]
        if elf_path in properly_parsed:
          elf_inputs = _ParseNinjaPathList(srcs)
  return sub_ninjas, elf_inputs


def ParseOneFileForTest(lines, dep_map, elf_path):
  return _ParseOneFile(lines, dep_map, elf_path)


def Parse(output_directory, elf_path):
  """Parses build.ninja and subninjas.

  Args:
    output_directory: Where to find the root build.ninja.
    elf_path: Path to elf file to find inputs for.

  Returns: A tuple of (source_mapper, elf_inputs).
  """
  if elf_path:
    elf_path = os.path.relpath(elf_path, output_directory)
  to_parse = ['build.ninja']
  seen_paths = set(to_parse)
  dep_map = {}
  elf_inputs = None
  while to_parse:
    path = os.path.join(output_directory, to_parse.pop())
    with open(path) as obj:
      sub_ninjas, found_elf_inputs = _ParseOneFile(obj, dep_map, elf_path)
      if found_elf_inputs:
        assert not elf_inputs, 'Found multiple inputs for elf_path ' + elf_path
        elf_inputs = found_elf_inputs
    for subpath in sub_ninjas:
      assert subpath not in seen_paths, 'Double include of ' + subpath
      seen_paths.add(subpath)
    to_parse.extend(sub_ninjas)

  return _SourceMapper(dep_map, len(seen_paths)), elf_inputs


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory', required=True)
  parser.add_argument('--elf-path')
  parser.add_argument('--show-inputs', action='store_true')
  parser.add_argument('--show-mappings', action='store_true')
  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  source_mapper, elf_inputs = Parse(args.output_directory, args.elf_path)
  if not elf_inputs:
    elf_inputs = []

  print('Found {} elf_inputs, and {} source mappings'.format(
      len(elf_inputs), len(source_mapper._dep_map)))
  if args.show_inputs:
    print('elf_inputs:')
    print('\n'.join(elf_inputs))
  if args.show_mappings:
    print('object_path -> source_path:')
    for path in source_mapper.IterAllPaths():
      print('{} -> {}'.format(path, source_mapper.FindSourceForPath(path)))


if __name__ == '__main__':
  main()
