#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to collect stats about progress of adding @NullMarked."""

import argparse
import collections
import logging
import pathlib
import sys
import time

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[3]

_EXCLUDED_SUBDIRS = ('out', 'third_party')
_TEST_PATH_SUBSTRINGS = ('test', 'Test')

_SUBDIRS_FOR_STATS = [
    'base',
    'chrome',
    'android_webview',
    'components',
    'content',
    'clank',
]


def _collect_java_files(start_dir):
    path = pathlib.Path(start_dir)
    for file in path.glob('**/*.java'):
        file.resolve()
        if not file.is_file():
            continue
        # Ignore tests for now.
        if any(s in str(file.absolute()) for s in _TEST_PATH_SUBSTRINGS):
            continue
        # Ignore files in excluded subdirs.
        for excluded_subdir in _EXCLUDED_SUBDIRS:
            if (_SRC_ROOT / excluded_subdir) in file.parents:
                break
        else:
            yield file


def _check_if_marked(java_files):
    marked_all = set()
    unmarked_all = set()
    for path in java_files:
        data = path.read_text()
        if '@NullMarked' in data:
            marked_all.add(path)
        else:
            unmarked_all.add(path)

    return marked_all, unmarked_all


def _breadown_stats_by_subdir(marked_all, unmarked_all):
    marked_by_subdirs = collections.defaultdict(int)
    unmarked_by_subdirs = collections.defaultdict(int)
    for file in marked_all:
        for subdir in _SUBDIRS_FOR_STATS:
            if (_SRC_ROOT / subdir) in file.parents:
                marked_by_subdirs[subdir] += 1
    for file in unmarked_all:
        for subdir in _SUBDIRS_FOR_STATS:
            if (_SRC_ROOT / subdir) in file.parents:
                unmarked_by_subdirs[subdir] += 1
    return marked_by_subdirs, unmarked_by_subdirs


def _print_stats(marked_all, unmarked_all):
    marked_by_subdirs, unmarked_by_subdirs = _breadown_stats_by_subdir(
        marked_all, unmarked_all)
    count_marked = len(marked_all)
    count_unmarked = len(unmarked_all)
    total = count_marked + count_unmarked
    print(f'* Combined stats:')
    print(f'{count_marked}/{total} ({round(count_marked/total*100)}%) ' +
          'files were marked with @NullMarked.')
    print(f'{count_unmarked}/{total} ({round(count_unmarked/total*100)}%) ' +
          'files were not modified.')

    print(f'* Subdir stats:')
    for subdir in _SUBDIRS_FOR_STATS:
        subdir_marked_count = marked_by_subdirs[subdir]
        subdir_unmarked_count = unmarked_by_subdirs[subdir]
        subdir_total = subdir_marked_count + subdir_unmarked_count
        # Skip non-existent subdirs.
        if subdir_total == 0:
            continue
        subdir_marked_pct = round(subdir_marked_count / subdir_total * 100)
        subdir_unmarked_pct = round(subdir_unmarked_count / subdir_total * 100)
        print(f'For //{subdir}:')
        print(f'{subdir_marked_count}/{subdir_total} ({subdir_marked_pct}%) ' +
              'files were marked with @NullMarked.')
        print(
            f'{subdir_unmarked_count}/{subdir_total} ({subdir_unmarked_pct}%) '
            + 'files were not modified.')


def _read_file_list(filepath):
    with open(filepath, 'rt') as f:
        return (pathlib.Path(java_file.strip()) for java_file in f.readlines())


def _write_file_list(filepath, filelist):
    with open(filepath, 'wt') as f:
        f.writelines(f'{str(p)}\n' for p in filelist)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-C',
                        dest='src_dir',
                        default=_SRC_ROOT,
                        help='Path to CHROMIUM_SRC.')
    parser.add_argument('--unmarked-list-path',
                        help='Path to output the list of unmarked files.')
    parser.add_argument('--marked-list-path',
                        help='Path to output the list of unmarked files.')
    parser.add_argument(
        '--cached-file-list',
        help='Path to list of java files instead of walking the tree.')
    parser.add_argument(
        '--output-file-list',
        help='Path to output list of java files for use by --cached-file-list.'
    )
    parser.add_argument('-v', '--verbose', action='store_true')
    options = parser.parse_args(sys.argv[1:])

    logging_level = logging.INFO
    if options.verbose:
        logging_level = logging.DEBUG
    logging.basicConfig(level=logging_level)

    if options.cached_file_list and options.output_file_list:
        parser.error(
            'Cant pass in both --cached-file-list and --output-file-list')

    logging.info('Collecting java files')
    start = time.time()
    if options.cached_file_list:
        java_files = _read_file_list(options.cached_file_list)
    else:
        java_files = list(_collect_java_files(options.src_dir))
    logging.info(f'Collecting java files done in {time.time()-start:.1f}s')

    logging.info('Processing files')
    start = time.time()
    marked, unmarked = _check_if_marked(java_files)
    logging.info(f'Processing files files done in {time.time()-start:.1f}s')

    if options.unmarked_list_path:
        _write_file_list(options.unmarked_list_path, unmarked)

    if options.marked_list_path:
        _write_file_list(options.marked_list_path, marked)

    if options.output_file_list:
        _write_file_list(options.output_file_list, java_files)

    logging.info('Calculating stats')
    start = time.time()
    _print_stats(marked, unmarked)
    logging.info(f'Calculating stats done in {time.time()-start:.1f}s')


if __name__ == '__main__':
    sys.exit(main())
