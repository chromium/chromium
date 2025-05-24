#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to collect stats about progress of adding @NullMarked."""

import argparse
import collections
import csv
from datetime import date
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


def _is_test(file):
    return any(s in str(file.absolute()) for s in _TEST_PATH_SUBSTRINGS)


def _collect_java_files(start_dir):
    path = pathlib.Path(start_dir)
    for file in path.glob('**/*.java'):
        file.resolve()
        if not file.is_file():
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
    nomark_all = set()
    for path in java_files:
        data = path.read_text()
        marked = '@NullMarked' in data
        unmarked = '@NullUnmarked' in data
        if marked:
            marked_all.add(path)
        elif not unmarked:
            nomark_all.add(path)
        if unmarked:
            unmarked_all.add(path)

    return marked_all, nomark_all, unmarked_all


def _breadown_stats_by_subdir(files):
    ret = collections.defaultdict(int)
    for file in files:
        for subdir in _SUBDIRS_FOR_STATS:
            if (_SRC_ROOT / subdir) in file.parents:
                ret[subdir] += 1
    return ret


def _print_stats(marked_all, nomark_all, unmarked_all):
    marked_by_subdirs = _breadown_stats_by_subdir(marked_all)
    nomark_by_subdirs = _breadown_stats_by_subdir(nomark_all)
    unmarked_by_subdirs = _breadown_stats_by_subdir(unmarked_all)
    count_marked = len(marked_all)
    count_nomark = len(nomark_all)
    count_unmarked = len(unmarked_all)
    total = count_marked + count_nomark

    def stat(c, t):
        pct_string = str(round(c / t * 100)) if t != 0 else '-'
        return f'{c}/{t} ({pct_string}%)'

    print()
    print(f'Overall:')
    print(f'  @NullMarked:', stat(count_marked, total))
    print(f'  Neither:', stat(count_nomark, total))
    print(f'  @NullUnmarked:', stat(count_unmarked, total))
    print()
    print(f'By Directory (@NullMarked / Neither / @NullUnmarked):')
    for subdir in _SUBDIRS_FOR_STATS:
        subdir_marked_count = marked_by_subdirs[subdir]
        subdir_nomark_count = nomark_by_subdirs[subdir]
        subdir_unmarked_count = unmarked_by_subdirs[subdir]
        subdir_total = subdir_marked_count + subdir_nomark_count
        # Skip non-existent subdirs.
        if subdir_total == 0:
            continue
        print(f'  //{subdir}:', stat(subdir_marked_count, subdir_total), '/',
              stat(subdir_nomark_count, subdir_total), '/',
              stat(subdir_unmarked_count, subdir_total))


def _read_file_list(filepath):
    with open(filepath, 'rt') as f:
        return (pathlib.Path(java_file.strip()) for java_file in f.readlines())


def _write_file_list(filepath, filelist):
    sorted_filelist = sorted(filelist)
    with open(filepath, 'wt') as f:
        f.writelines(f'{str(p)}\n' for p in sorted_filelist)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-C',
                        dest='src_dir',
                        default=_SRC_ROOT,
                        help='Path to CHROMIUM_SRC.')
    parser.add_argument(
        '--unmarked-list-path',
        help='Path to output the list of files with @NullUnmarked.')
    parser.add_argument(
        '--marked-list-path',
        help='Path to output the list of files with @NullMarked.')
    parser.add_argument(
        '--nomark-list-path',
        help='Path to output the list of files without any annotation.')
    parser.add_argument(
        '--cached-file-list',
        help='Path to list of java files instead of walking the tree.')
    parser.add_argument(
        '--output-file-list',
        help='Path to output list of java files for use by --cached-file-list.'
    )
    parser.add_argument('--csv', action='store_true', help='Output a .csv')
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
    marked, nomark, unmarked = _check_if_marked(java_files)
    logging.info(f'Processing files files done in {time.time()-start:.1f}s')

    if options.unmarked_list_path:
        _write_file_list(options.unmarked_list_path, unmarked)
    if options.marked_list_path:
        _write_file_list(options.marked_list_path, marked)
    if options.nomark_list_path:
        _write_file_list(options.nomark_list_path, nomark)
    if options.output_file_list:
        _write_file_list(options.output_file_list, java_files)

    logging.info('Calculating stats')
    start = time.time()
    marked_tests = {x for x in marked if _is_test(x)}
    marked.difference_update(marked_tests)
    nomark_tests = {x for x in nomark if _is_test(x)}
    nomark.difference_update(nomark_tests)
    unmarked_tests = {x for x in unmarked if _is_test(x)}
    unmarked.difference_update(unmarked_tests)

    if options.csv:
        csv.writer(sys.stdout).writerow(
            (date.today(), len(marked), len(nomark), len(unmarked),
             len(marked_tests), len(nomark_tests), len(unmarked_tests)))
    else:
        print(date.today())
        print('==== Non-test Files ====')
        _print_stats(marked, nomark, unmarked)
        print()
        print('====== Test Files ======')
        _print_stats(marked_tests, nomark_tests, unmarked_tests)
    logging.info(f'Calculating stats done in {time.time()-start:.1f}s')


if __name__ == '__main__':
    sys.exit(main())
