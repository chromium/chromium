#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
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
import logging
import pathlib
import sys

_SRC_DIR = pathlib.Path(__file__).resolve().parents[4]

sys.path.append(str(_SRC_DIR / 'build/android'))
from pylib import constants

sys.path.append(str(_SRC_DIR / 'build/android/gyp'))
from util import dep_utils


def main():
  arg_parser = argparse.ArgumentParser(
      description='Finds which build target contains a particular Java class.')

  arg_parser.add_argument('-C',
                          '--output-directory',
                          help='Build output directory.')
  arg_parser.add_argument('--build',
                          action='store_true',
                          help='Build all .build_config files.')
  arg_parser.add_argument('classes',
                          nargs='+',
                          help='Java classes to search for')
  arg_parser.add_argument('-v',
                          '--verbose',
                          action='store_true',
                          help='Verbose logging.')

  arguments = arg_parser.parse_args()

  logging.basicConfig(
      level=logging.DEBUG if arguments.verbose else logging.WARNING,
      format='%(asctime)s.%(msecs)03d %(levelname).1s %(message)s',
      datefmt='%H:%M:%S')

  if arguments.output_directory:
    constants.SetOutputDirectory(arguments.output_directory)
  constants.CheckOutputDirectory()
  abs_out_dir: pathlib.Path = pathlib.Path(
      constants.GetOutDirectory()).resolve()

  index = dep_utils.ClassLookupIndex(abs_out_dir, arguments.build)
  matches = {c: index.match(c) for c in arguments.classes}

  if not arguments.build:
    # Try finding match without building because it is faster.
    for class_name, match_list in matches.items():
      if len(match_list) == 0:
        arguments.build = True
        break
    if arguments.build:
      index = dep_utils.ClassLookupIndex(abs_out_dir, True)
      matches = {c: index.match(c) for c in arguments.classes}

  if not arguments.build:
    print('Showing potentially stale results. Run lookup.dep.py with --build '
          '(slower) to build any unbuilt GN targets and get full results.')
    print()

  for (class_name, class_entries) in matches.items():
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


if __name__ == '__main__':
  main()
