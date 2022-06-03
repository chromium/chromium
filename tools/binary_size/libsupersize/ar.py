#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic for reading .a files."""

import argparse
import logging
import os


def IsThinArchive(path):
  """Returns whether the given .a is a thin archive."""
  with open(path, 'rb') as f:
    return f.read(8) == b'!<thin>\n'


def CreateThinObjectPath(archive_path, subpath):
  """Given the .a path and .o subpath, returns the .o path."""
  # |subpath| is path complete under Gold, and incomplete under LLD. Check its
  # prefix to test completeness, and if not, use |archive_path| to supply the
  # required prefix.
  if subpath.startswith('obj/'):
    return subpath
  # .o subpaths in thin archives are relative to the directory of the .a.
  parent_path = os.path.dirname(archive_path)
  return os.path.normpath(os.path.join(parent_path, subpath))


def IterArchiveChunks(path):
  """For each .o embedded in the given .a file, yields (foo.o, foo_contents).

  Args:
    path: Path to .a file.

  Returns:
    A (str, bytes) tuple, specifying (.o file entry, None or payload data).
  """
  # File format reference:
  # https://github.com/pathscale/binutils/blob/master/gold/archive.cc
  with open(path, 'rb') as f:
    header = f.read(8)
    is_thin = header == b'!<thin>\n'
    if not is_thin and header != b'!<arch>\n':
      raise Exception('Invalid .a: ' + path)

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

      # Although entry_size exists for thin archives, no data exists in the .a.
      entry_data = None if is_thin else read_payload(entry_size)
      yield entry_name.rstrip(b'/').decode('ascii'), entry_data


def ExpandThinArchives(paths, output_directory):
  """Expands all thin archives found in |paths| into .o paths.

  Args:
    paths: List of paths relative to |output_directory|.
    output_directory: Output directory.

  Returns:
    * A new list of paths with all thin archives replaced by .o paths.
    * A set of all .a paths that were thin archives.
  """
  expanded_paths = []
  thin_paths = set()
  num_archives = 0
  for path in paths:
    if not path.endswith('.a'):
      expanded_paths.append(path)
      continue
    num_archives += 1
    abs_path = os.path.join(output_directory, path)
    if not IsThinArchive(abs_path):
      expanded_paths.append(path)
      continue

    thin_paths.add(path)
    # Thin archive.
    for subpath, _ in IterArchiveChunks(abs_path):
      expanded_paths.append(CreateThinObjectPath(path, subpath))

  logging.info('%d of %d .a files were thin archives',
               len(thin_paths), num_archives)
  return expanded_paths, thin_paths


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('ar_path')
  parser.add_argument('--expand-thin', action='store_true')
  parser.add_argument('--output-directory', default='.')
  args = parser.parse_args()

  if args.expand_thin:
    expanded = ExpandThinArchives([args.ar_path], args.output_directory)[0]
    print('\n'.join(expanded))
  else:
    for name, payload in IterArchiveChunks(args.ar_path):
      print('{}: size={}'.format(name, len(payload) if payload else '<thin>'))


if __name__ == '__main__':
  main()
