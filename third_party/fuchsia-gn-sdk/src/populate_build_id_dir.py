#!/usr/bin/env python3.8
#
# Copyright 2020 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Populates a .build-id directory of unstripped binaries cataloged by a list
of ids.txt files."""

import argparse
import os
import shutil
import subprocess
import sys


def populate_build_id_dir(readelf_exec, ids_txt_path, output_dir, build_id_dirs, filenames):
    """
  Processes an ids.txt file, populating the output .build-id directory.

  Each unstripped ELF binary is placed in a hierarchy keyed by the GNU build
  ID. Each binary resides in a directory whose name is the first two characters
  of the build ID, with the binary file itself named after the remaining
  characters of the build ID. So, a binary file with the build ID "deadbeef"
  would be located at the path 'output_dir/de/adbeef.debug'.

  See documentation at https://fedoraproject.org/wiki/Releases/FeatureBuildId.

  Args:
    ids_txt_path (str): Path to ids.txt file to process.
    output_dir (str): Path to output .build-id directory.
    build_id_dirs (list of str): Paths to directory containing symbols for
      prebuilts.
    filenames (set): Set to store binary filenames which were hardlinked to
      the output .build-id directory.
  """
    for line in open(ids_txt_path, 'r'):
        build_id, binary_path = line.strip().split(' ')
        output_filename = os.path.join(build_id[:2], build_id[2:] + '.debug')

        # Check first if we have the debug binary under any |build_id_dirs|.
        symbol_source_path = _find_binary_file(output_filename, build_id_dirs)

        # Otherwise, check in the ids.txt directory itself, assuming
        # relative paths in ids.txt.
        if not symbol_source_path:
            symbol_source_path = os.path.abspath(
                os.path.join(os.path.dirname(ids_txt_path), binary_path))

        # Don't check zero length files, they exist as placeholders for prebuilts.
        if os.path.getsize(symbol_source_path) == 0:
            continue

        # Exclude stripped binaries (indicated by their lack of symbol tables).
        readelf_args = [readelf_exec, '-S', symbol_source_path]
        readelf_output = subprocess.check_output(
            readelf_args, universal_newlines=True, text=True)
        if '.symtab' not in readelf_output:
            continue

        output_path = os.path.join(output_dir, output_filename)
        if not os.path.exists(os.path.dirname(output_path)):
            os.makedirs(os.path.dirname(output_path))
        if not os.path.exists(output_path):
            os.link(symbol_source_path, output_path)
        filenames.add(output_path)


def _find_binary_file(binary_file, build_id_dirs):
    """Look for the binary_file in the list of build_id_dirs."""
    for dir in build_id_dirs:
        filepath = os.path.join(dir, binary_file)
        if os.path.exists(filepath):
            return filepath
    return None


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'ids_txt_paths',
        type=str,
        help='Path to a file, which is a newline-separated list '
        'of paths to ids.txt files.')
    parser.add_argument(
        '--output_dir',
        type=str,
        required=True,
        help='Path to output .build-id dir.')
    parser.add_argument(
        '--build-id-dir',
        type=str,
        required=True,
        action='append',
        help='Directory containing symbols. Can be specified multiple times')
    parser.add_argument(
        '--depfile', type=str, required=True, help='Path to the depfile.')
    parser.add_argument(
        '--stamp', type=str, required=True, help='Path to stamp file.')
    parser.add_argument(
        '--readelf-exec', default='readelf', help='readelf executable to use.')
    args = parser.parse_args(args)

    # If the output directory already exists, wipe it, so the directory does
    # not accumulate over multiple invocations. This is important since the
    # same executable will have a different name when its content changes.
    if os.path.exists(args.output_dir):
        shutil.rmtree(args.output_dir)

    with open(args.ids_txt_paths, 'r') as f:
        ids_txt_paths = f.read().splitlines()

    filenames = set()
    for ids_txt_path in ids_txt_paths:
        populate_build_id_dir(args.readelf_exec, ids_txt_path, args.output_dir, args.build_id_dir, filenames)

    with open(args.depfile, 'w') as f:
        f.writelines('%s: %s\n' % (args.stamp, ' '.join(sorted(filenames))))
    with open(args.stamp, 'w') as f:
        os.utime(f.name, None)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
