#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract source file information from .ninja files."""

import argparse
import logging
import os
import re
import sys


# E.g.:
# build obj/.../foo.o: cxx gen/.../foo.cc || obj/.../foo.inputdeps.stamp
# build obj/.../libfoo.a: alink obj/.../a.o obj/.../b.o |
# build ./libchrome.so ./lib.unstripped/libchrome.so: solink a.o b.o ...
# build libmonochrome.so: __chrome_android_libmonochrome___rule | ...
_REGEX = re.compile(r'build ([^:]+): \w+ (.*?)(?: *\||\n|$)')

_RLIBS_REGEX = re.compile(r'  rlibs = (.*?)(?:\n|$)')


class _SourceMapper:

  def __init__(self, dep_map, parsed_file_count, inputs_by_path):
    self._dep_map = dep_map
    self.parsed_file_count = parsed_file_count
    self._inputs_by_path = inputs_by_path
    self.inputs_map_count = len(inputs_by_path)
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
      if lib_name.endswith('rlib') and 'std/' in lib_name:
        # Currently we use binary prebuilt static libraries of the Rust
        # stdlib so we can't get source paths. That may change in future.
        return '(Rust stdlib)/%s' % lib_name
      return None
    if lib_name.endswith('.rlib'):
      # Rust doesn't really have the concept of an object file because
      # the compilation unit is the whole 'crate'. Return whichever
      # filename was the crate root.
      return next(iter(by_basename.values()))
    obj_path = by_basename.get(obj_name)
    if not obj_path:
      # Found the library, but it doesn't list the .o file.
      logging.warning('no obj basename for %s %s', path, obj_name)
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
    return self._dep_map.keys()

  def GetInputsForBinary(self, path):
    return self._inputs_by_path.get(path)


def _ParseNinjaPathList(path_list):
  ret = path_list.replace('\\ ', '\b')
  return [s.replace('\b', ' ') for s in ret.split()]


def _OutputsAreObject(outputs):
  return (outputs.endswith('.a') or outputs.endswith('.o')
          or outputs.endswith('.rlib'))


def _ParseOneFile(lines, dep_map, inputs_by_path):
  sub_ninjas = []
  last_elf_paths = []
  for line in lines:
    if line.startswith('subninja '):
      sub_ninjas.append(line[9:-1])
      continue
    m = _REGEX.match(line)
    if m:
      outputs, srcs = m.groups()
      if _OutputsAreObject(outputs):
        output = outputs.replace('\\ ', ' ')
        assert output not in dep_map, 'Duplicate output: ' + output
        if output[-1] == 'o':
          dep_map[output] = srcs.replace('\\ ', ' ')
        else:
          obj_paths = _ParseNinjaPathList(srcs)
          dep_map[output] = {os.path.basename(p): p for p in obj_paths}
      elif inputs_by_path:
        last_elf_paths = [
            os.path.normpath(p) for p in _ParseNinjaPathList(outputs)
        ]
        for elf_path in last_elf_paths:
          results = inputs_by_path.get(elf_path)
          if results is not None:
            results.extend(_ParseNinjaPathList(srcs))
    # Rust .rlibs are listed as implicit dependencies of the main
    # target linking rule, then are given as an extra
    #   rlibs =
    # variable on a subsequent line. Watch out for that line.
    m = _RLIBS_REGEX.match(line)
    if m:
      for elf_path in last_elf_paths:
        results = inputs_by_path.get(elf_path)
        if results is not None:
          results.extend(_ParseNinjaPathList(m.group(1)))

  return sub_ninjas


def ParseOneFileForTest(lines, dep_map, inputs_by_path):
  return _ParseOneFile(lines, dep_map, inputs_by_path)


def Parse(output_directory, interesting_elf_paths):
  """Parses build.ninja and subninjas.

  Args:
    output_directory: Where to find the root build.ninja.
    interesting_elf_paths: Paths of link steps to collect inputs for.

  Returns: A tuple of (source_mapper, inputs_by_path).
  """
  if interesting_elf_paths:
    # Change paths to be relative to output directory.
    inputs_by_path = {
        os.path.relpath(p, output_directory): []
        for p in interesting_elf_paths
    }
  else:
    inputs_by_path = {}
  to_parse = ['build.ninja']
  seen_paths = set(to_parse)
  dep_map = {}
  while to_parse:
    path = os.path.join(output_directory, to_parse.pop())
    with open(path, encoding='utf-8', errors='ignore') as obj:
      sub_ninjas = _ParseOneFile(obj, dep_map, inputs_by_path)
    for subpath in sub_ninjas:
      assert subpath not in seen_paths, 'Double include of ' + subpath
      seen_paths.add(subpath)
    to_parse.extend(sub_ninjas)

  # Change paths back to their original forms and remove empty entries.
  ret_inputs_by_path = {}
  for path in interesting_elf_paths:
    if inputs := inputs_by_path.get(os.path.relpath(path, output_directory)):
      ret_inputs_by_path[path] = inputs
  return _SourceMapper(dep_map, len(seen_paths), ret_inputs_by_path)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory', required=True)
  parser.add_argument('--elf-path')
  parser.add_argument('--show-inputs', action='store_true')
  parser.add_argument('--show-mappings', action='store_true')
  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  elf_paths = [args.elf_path] if args.elf_path else None
  source_mapper = Parse(args.output_directory, elf_paths)
  elf_inputs = source_mapper.GetInputsForBinary(args.elf_path)

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
