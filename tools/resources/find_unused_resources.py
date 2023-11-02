#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script searches for unused art assets listed in a .grd file.

It uses git grep to look for references to the IDR resource id or the base
filename. If neither is found, the file is reported unused.

Requires a git checkout. Must be run from your checkout's "src" root.

Example:
  cd /work/chrome/src
  tools/resources/find_unused_resouces.py chrome/browser/browser_resources.grd
"""

from __future__ import print_function

__author__ = 'jamescook@chromium.org (James Cook)'


import os
import re
import subprocess
import sys


def GetBaseResourceId(resource_id):
  """Removes common suffixes from a resource ID.

  Removes suffixies that may be added by macros like IMAGE_GRID or IMAGE_BORDER.
  For example, converts IDR_FOO_LEFT and IDR_FOO_RIGHT to just IDR_FOO.

  Args:
    resource_id: String resource ID.

  Returns:
    A string with the base part of the resource ID.
  """
  suffixes = [
      '_TOP_LEFT', '_TOP', '_TOP_RIGHT',
      '_LEFT', '_CENTER', '_RIGHT',
      '_BOTTOM_LEFT', '_BOTTOM', '_BOTTOM_RIGHT',
      '_TL', '_T', '_TR',
      '_L', '_M', '_R',
      '_BL', '_B', '_BR']
  # Note: This does not check _HOVER, _PRESSED, _HOT, etc. as those are never
  # used in macros.
  for suffix in suffixes:
    if resource_id.endswith(suffix):
      resource_id = resource_id[:-len(suffix)]
  return resource_id


def FindFilesWithContents(string_a, string_b):
  """Returns list of paths of files that contain |string_a| or |string_b|.

  Uses --name-only to print the file paths. The default behavior of git grep
  is to OR together multiple patterns.

  Args:
    string_a: A string to search for (not a regular expression).
    string_b: As above.

  Returns:
    A list of file paths as strings.
  """
  matching_files = subprocess.check_output([
      'git', 'grep', '--name-only', '--fixed-strings', '-e', string_a,
      '-e', string_b])
  files_list = matching_files.split('\n')
  # The output ends in a newline, so slice that off.
  files_list = files_list[:-1]
  return files_list


def GetUnusedResources(grd_filepath):
  """Returns a list of resources that are unused in the code.

  Prints status lines to the console because this function is quite slow.

  Args:
    grd_filepath: Path to a .grd file listing resources.

  Returns:
    A list of pairs of [resource_id, filepath] for the unused resources.
  """
  unused_resources = []
  grd_file = open(grd_filepath, 'r')
  grd_data = grd_file.read()
  print('Checking:')
  # Match the resource id and file path out of substrings like:
  # ...name="IDR_FOO_123" file="common/foo.png"...
  # by matching between the quotation marks.
  pattern = re.compile(
      r"""name="([^"]*)"  # Match resource ID between quotes.
      \s*                 # Run of whitespace, including newlines.
      file="([^"]*)"      # Match file path between quotes.""",
      re.VERBOSE)
  # Use finditer over the file contents because there may be newlines between
  # the name and file attributes.
  searched = set()
  for result in pattern.finditer(grd_data):
    # Extract the IDR resource id and file path.
    resource_id = result.group(1)
    filepath = result.group(2)
    filename = os.path.basename(filepath)
    base_resource_id = GetBaseResourceId(resource_id)

    # Do not bother repeating searches.
    key = (base_resource_id, filename)
    if key in searched:
      continue
    searched.add(key)

    # Print progress as we go along.
    print(resource_id)

    # Ensure the resource isn't used anywhere by checking both for the resource
    # id (which should appear in C++ code) and the raw filename (in case the
    # file is referenced in a script, test HTML file, etc.).
    matching_files = FindFilesWithContents(base_resource_id, filename)

    # Each file is matched once in the resource file itself. If there are no
    # other matching files, it is unused.
    if len(matching_files) == 1:
      # Give the user some happy news.
      print('Unused!')
      unused_resources.append([resource_id, filepath])

  return unused_resources


def GetScaleDirectories(resources_path):
  """Returns a list of paths to per-scale-factor resource directories.

  Assumes the directory names end in '_percent', for example,
  ash/resources/default_200_percent or
  chrome/app/theme/resources/touch_140_percent

  Args:
    resources_path: The base path of interest.

  Returns:
    A list of paths relative to the 'src' directory.
  """
  file_list = os.listdir(resources_path)
  scale_directories = []
  for file_entry in file_list:
    file_path = os.path.join(resources_path, file_entry)
    if os.path.isdir(file_path) and file_path.endswith('_percent'):
      scale_directories.append(file_path)

  scale_directories.sort()
  return scale_directories


def main():
  # The script requires exactly one parameter, the .grd file path.
  if len(sys.argv) != 2:
    print('Usage: tools/resources/find_unused_resources.py <path/to/grd>')
    sys.exit(1)
  grd_filepath = sys.argv[1]

  # Try to ensure we are in a source checkout.
  current_dir = os.getcwd()
  if os.path.basename(current_dir) != 'src':
    print('Script must be run in your "src" directory.')
    sys.exit(1)

  # We require a git checkout to use git grep.
  if not os.path.exists(current_dir + '/.git'):
    print('You must use a git checkout for this script to run.')
    print(current_dir + '/.git', 'not found.')
    sys.exit(1)

  # Look up the scale-factor directories.
  resources_path = os.path.dirname(grd_filepath)
  scale_directories = GetScaleDirectories(resources_path)
  if not scale_directories:
    print('No scale directories (like "default_100_percent") found.')
    sys.exit(1)

  # |unused_resources| stores pairs of [resource_id, filepath] for resource ids
  # that are not referenced in the code.
  unused_resources = GetUnusedResources(grd_filepath)
  if not unused_resources:
    print('All resources are used.')
    sys.exit(0)

  # Dump our output for the user.
  print()
  print('Unused resource ids:')
  for resource_id, filepath in unused_resources:
    print(resource_id)
  # Print a list of 'git rm' command lines to remove unused assets.
  print()
  print('Unused files:')
  for resource_id, filepath in unused_resources:
    for directory in scale_directories:
      print('git rm ' + os.path.join(directory, filepath))


if __name__ == '__main__':
  main()
