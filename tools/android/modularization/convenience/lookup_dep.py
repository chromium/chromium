#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r'''Finds which build target(s) contain a particular Java class.

This is a utility script for finding out which build target dependency needs to
be added to import a given Java class.

It is a best-effort script.

Example:

Find build target with class FooUtil:
   tools/android/modularization/convenience/lookup_dep.py FooUtil
'''

import argparse
import collections
import dataclasses
import json
import logging
import os
import pathlib
import subprocess
import sys
from typing import Dict, List

_SRC_DIR = pathlib.Path(__file__).parents[4].resolve()

sys.path.append(str(_SRC_DIR / 'build' / 'android'))
from pylib import constants


def main():
  arg_parser = argparse.ArgumentParser(
      description='Finds which build target contains a particular Java class.')

  arg_parser.add_argument('-C',
                          '--output-directory',
                          help='Build output directory.')
  arg_parser.add_argument('classes',
                          nargs='+',
                          help=f'Java classes to search for')
  arg_parser.add_argument('-v',
                          '--verbose',
                          action='store_true',
                          help=f'Verbose logging.')

  arguments = arg_parser.parse_args()

  logging.basicConfig(
      level=logging.DEBUG if arguments.verbose else logging.WARNING,
      format='%(asctime)s.%(msecs)03d %(levelname).1s %(message)s',
      datefmt='%H:%M:%S')

  if arguments.output_directory:
    constants.SetOutputDirectory(arguments.output_directory)
  constants.CheckOutputDirectory()
  out_dir: str = constants.GetOutDirectory()

  index = ClassLookupIndex(pathlib.Path(out_dir))
  for class_name in arguments.classes:
    class_entries = index.match(class_name)
    if not class_entries:
      print(f'Could not find build target for class "{class_name}"')
    elif len(class_entries) == 1:
      class_entry = class_entries[0]
      print(f'Class {class_entry.full_class_name} found:')
      print(f'    "{class_entry.target}"')
    else:
      print(f'Multiple targets with classes that match "{class_name}":')
      print()
      for class_entry in class_entries:
        print(f'    "{class_entry.target}"')
        print(f'        contains {class_entry.full_class_name}')
        print()


@dataclasses.dataclass(frozen=True)
class ClassEntry:
  """An assignment of a Java class to a build target."""
  full_class_name: str
  target: str


class ClassLookupIndex:
  """A map from full Java class to its build targets.

  A class might be in multiple targets if it's bytecode rewritten."""

  def __init__(self, build_output_dir: pathlib.Path):
    self._build_output_dir = build_output_dir
    self._class_index = self._index_root()

  def match(self, search_string: str) -> List[ClassEntry]:
    """Get class/target entries where the class matches search_string"""
    # Priority 1: Exact full matches
    if search_string in self._class_index:
      return self._entries_for(search_string)

    # Priority 2: Match full class name (any case), if it's a class name
    matches = []
    lower_search_string = search_string.lower()
    if '.' not in lower_search_string:
      for full_class_name in self._class_index:
        package_and_class = full_class_name.rsplit('.', 1)
        if len(package_and_class) < 2:
          continue
        class_name = package_and_class[1]
        class_lower = class_name.lower()
        if class_lower == lower_search_string:
          matches.extend(self._entries_for(full_class_name))
      if matches:
        return matches

    # Priority 3: Match anything
    for full_class_name in self._class_index:
      if lower_search_string in full_class_name.lower():
        matches.extend(self._entries_for(full_class_name))

    return matches

  def _entries_for(self, class_name) -> List[ClassEntry]:
    return [
        ClassEntry(class_name, target)
        for target in self._class_index.get(class_name)
    ]

  def _index_root(self) -> Dict[str, List[str]]:
    """Create the class to target index."""
    logging.debug('Running list_java_targets.py...')
    list_java_targets_command = [
        'build/android/list_java_targets.py', '--type=java_library',
        '--gn-labels', '--build', '--print-build-config-paths',
        f'--output-directory={self._build_output_dir}'
    ]
    list_java_targets_run = subprocess.run(list_java_targets_command,
                                           cwd=_SRC_DIR,
                                           capture_output=True,
                                           text=True,
                                           check=True)
    logging.debug('... done.')

    # Parse output of list_java_targets.py with mapping of build_target to
    # build_config
    root_build_targets = list_java_targets_run.stdout.split('\n')
    class_index = collections.defaultdict(list)
    for target_line in root_build_targets:
      # Skip empty lines
      if not target_line:
        continue

      target_line_parts = target_line.split(': ')
      assert len(target_line_parts) == 2, target_line_parts
      target, build_config_path = target_line_parts

      # Read the location of the java_sources_file from the build_config
      with open(build_config_path) as build_config_contents:
        build_config: Dict = json.load(build_config_contents)
      deps_info = build_config['deps_info']
      sources_path = deps_info.get('java_sources_file')
      if not sources_path:
        # TODO(crbug.com/1108362): Handle targets that have no
        # deps_info.sources_path but contain srcjars.
        continue

      # Read the java_sources_file, indexing the classes found
      with open(self._build_output_dir / sources_path) as sources_contents:
        sources_lines = sources_contents
        for source_line in sources_lines:
          source_path = pathlib.Path(source_line.strip())
          java_class = self._parse_full_java_class(source_path)
          if java_class:
            class_index[java_class].append(target)
          continue

    return class_index

  def _parse_full_java_class(self, source_path: pathlib.Path) -> str:
    """Guess the fully qualified class name from the path to the source file."""
    if source_path.suffix != '.java':
      logging.warning(f'"{source_path}" does not have the .java suffix')
      return None

    directory_path: pathlib.Path = source_path.parent
    package_list_reversed = []
    for part in reversed(directory_path.parts):
      package_list_reversed.append(part)
      if part in ('com', 'org'):
        break
    else:
      logging.debug(f'File {source_path} not in a subdir of "org" or "com", '
                    'cannot detect package heuristically.')
      return None

    package = '.'.join(reversed(package_list_reversed))
    class_name = source_path.stem
    return f'{package}.{class_name}'


if __name__ == '__main__':
  main()
