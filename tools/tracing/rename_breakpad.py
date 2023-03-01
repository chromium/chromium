# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Renames breakpad file to the standard module id format.

See perfetto::profiling::BreakpadSymbolizer for more naming information.
"""

import logging
import os
import shutil

import flag_utils


def RenameBreakpadFiles(breakpad_dir, breakpad_output_dir):
  """Move breakpad files to new directory and rename them.

  Breakpad files (files that contain '.breakpad') are renamed
  to follow a '<module_id>.breakpad' with upper-case hexadecimal
  naming scheme and moved to the |breakpad_output_dir| directory.
  See perfetto::profiling::BreakpadSymbolizer for more naming
  information. All other non-breakpad or misformatted breakpad files
  remain in the same directory with the same filename.

  Args:
    breakpad_dir: local directory that stores symbol files in its subtree.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    AssertionError: if a file's module_id is None or repeated by
      another file.
  """
  # Runs on every directory in the subtree. Scans directories from top-down
  # (root to leaves) so that we don't rename files multiple times in the common
  # case where |breakpad_dir| = |breakpad_output_dir|.
  flag_utils.GetTracingLogger().debug('Renaming breakpad files.')
  for subdir_path, _, filenames in os.walk(breakpad_dir, topdown=True):
    for filename in filenames:
      file_path = os.path.abspath(os.path.join(subdir_path, filename))

      if '.breakpad' not in filename:
        flag_utils.GetTracingLogger().debug('File is not a breakpad file: %s',
                                            file_path)
        continue

      module_id = ExtractModuleIdIfValidBreakpad(file_path)
      if module_id is None:
        flag_utils.GetTracingLogger().debug(
            'Failed to extract file module id: %s', file_path)
        continue

      new_filename = module_id + '.breakpad'
      dest_path = os.path.abspath(
          os.path.join(breakpad_output_dir, new_filename))

      # Ensure all new filenames (module ids) are unique. If there is module id
      # repetition, the first file with the same module_id has already been
      # moved.
      if os.path.exists(dest_path):
        raise AssertionError(('Symbol file modules ids are not '
                              'unique: %s\nSee these files: %s, %s' %
                              (module_id, file_path, dest_path)))

      shutil.move(file_path, dest_path)


def ExtractModuleIdIfValidBreakpad(file_path):
  """Extracts breakpad file's module id if the file is valid.

  A breakpad file is valid for extracting its module id if it
  has a valid MODULE record, formatted like so:
  MODULE operatingsystem architecture id name

  For example:
  MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name

  See this for more information:
  https://chromium.googlesource.com/breakpad/breakpad/+/HEAD/docs/symbol_files.md#records-1

  Args:
    file_path: Path to breakpad file to extract module id from.

  Returns:
    Module id if file is a valid breakpad file; None, otherwise.
  """
  module_id = None
  with open(file_path, 'r', encoding='utf-8') as file_handle:
    # Reads a maximum of 200 bytes/characters. Malformed file or binary will
    # not have '\n' character.
    first_line = file_handle.readline(200)
    fragments = first_line.rstrip().split()
    if fragments and fragments[0] == 'MODULE' and len(fragments) >= 5:
      # Symbolization script's input file format requires module id hexadecimal
      # to be upper case.
      module_id = fragments[3].upper()

  return module_id
