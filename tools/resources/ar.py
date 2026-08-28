#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logic for reading .a files.

Copied from //tools/binary_size/libsupersize/ar.py"""

import os


def _ResolveThinObjectPath(archive_path, subpath):
  """Given the .a path and .o subpath, returns the .o path."""
  # |subpath| is path complete under Gold, and incomplete under LLD. Check its
  # prefix to test completeness, and if not, use |archive_path| to supply the
  # required prefix.
  if subpath.startswith('obj/'):
    return subpath
  # .o subpaths in thin archives are relative to the directory of the .a.
  parent_path = os.path.dirname(archive_path)
  return os.path.normpath(os.path.join(parent_path, subpath))


def _IterThinPaths(path):
  """Given the .a path, yields all nested .o paths."""
  # File format reference:
  # https://github.com/pathscale/binutils/blob/master/gold/archive.cc
  with open(path, 'rb') as f:
    header = f.read(8)
    is_thin = header == b'!<thin>\n'
    if not is_thin and header != b'!<arch>\n':
      raise Exception('Invalid .a: ' + path)
    if not is_thin:
      return

    def read_payload(size):
      ret = f.read(size)
      # Entries are 2-byte aligned.
      if size & 1 != 0:
        f.read(1)
      return ret

    while True:
      entry = f.read(60)
      if not entry:
        return
      entry_name = entry[:16].rstrip()
      entry_size = int(entry[48:58].rstrip())

      if entry_name in (b'', b'/', b'//', b'/SYM64/'):
        payload = read_payload(entry_size)
        # Metadata sections we don't care about.
        if entry_name == b'//':
          name_list = payload
        continue

      if entry_name[0:1] == b'/':
        # Name is specified as location in name table.
        # E.g.: /123
        name_offset = int(entry_name[1:])
        # String table enties are delimited by \n (e.g. "browser.o/\n").
        end_idx = name_list.index(b'\n', name_offset)
        entry_name = name_list[name_offset:end_idx]
      else:
        # Name specified inline with spaces for padding (e.g. "browser.o/    ").
        entry_name = entry_name.rstrip()

      yield entry_name.rstrip(b'/').decode('ascii')


def ExpandThinArchives(paths):
  """Expands all thin archives found in |paths| into .o paths.

  Args:
    paths: List of paths relative to |output_directory|.
    output_directory: Output directory.

  Returns:
    * A new list of paths with all archives replaced by .o paths.
  """
  expanded_paths = []
  for path in paths:
    if not path.endswith('.a'):
      expanded_paths.append(path)
      continue

    with open(path, 'rb') as f:
      header = f.read(8)
      is_thin = header == b'!<thin>\n'
      if is_thin:
        for subpath in _IterThinPaths(path):
          expanded_paths.append(_ResolveThinObjectPath(path, subpath))
      elif header != b'!<arch>\n':
        raise Exception('Invalid .a: ' + path)

  return expanded_paths
