#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import functools
import logging
import multiprocessing
import os
import pathlib
import subprocess
import sys
from typing import Optional

import json_gn_editor
import utils

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).parents[2].resolve()
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

_GIT_IGNORE_STR = '(git ignored file) '


def _split_dep(existing_dep: str, new_dep: str, root: pathlib.Path,
               filepath: str, dryrun: bool) -> str:
    with json_gn_editor.BuildFile(filepath, root, dryrun=dryrun) as build_file:
        if build_file.split_dep(existing_dep, new_dep):
            ignore_string = ''
            if utils.is_git_ignored(root, filepath):
                ignore_string = _GIT_IGNORE_STR
            dryrun_string = '[DRYRUN] ' if dryrun else ''
            relpath = os.path.relpath(filepath, start=root)
            return f'{dryrun_string}Updated {ignore_string}' + relpath
    return ''


def main():
    parser = argparse.ArgumentParser(
        description='Add a new dep to all build targets that depend on an '
        'existing dep.')
    parser.add_argument('existing_dep', help='The dep to split from.')
    parser.add_argument('new_dep', help='The new dep to add.')
    parser.add_argument(
        '-n',
        '--dryrun',
        action='store_true',
        help='Show which files would be updated but avoid changing them.')
    parser.add_argument(
        '-v',
        '--verbose',
        action='count',
        default=0,
        help='1 to print logging, 2 to print each build file checked.')

    args = parser.parse_args()
    if args.verbose >= 2:
        level = logging.DEBUG
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.WARNING
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    build_filepaths = []
    root = git_metadata_utils.get_chromium_src_path()
    logging.info('Finding build files under %s', root)
    for dirpath, dirnames, filenames in os.walk(root):
        for filename in filenames:
            filepath = os.path.join(dirpath, filename)
            if utils.is_bad_gn_file(filepath):
                continue
            if filename.endswith('.gn') or filename.endswith('.gni'):
                build_filepaths.append(filepath)

    build_filepaths.sort()
    num_total = len(build_filepaths)
    logging.info('Found %d build files.', num_total)

    updated_files = []
    ignored_files = []
    with multiprocessing.Pool() as pool:
        tasks = {
            filepath: pool.apply_async(
                _split_dep,
                (args.existing_dep, args.new_dep, root, filepath, args.dryrun))
            for filepath in build_filepaths
        }
        for idx, filepath in enumerate(tasks.keys()):
            logging.debug('[%d/%d] Checking %s', idx, num_total, filepath)
            return_value = tasks[filepath].get()
            if return_value:
                logging.info(return_value)
                updated_files.append(filepath)
                if _GIT_IGNORE_STR in return_value:
                    ignored_files.append(filepath)
    num_updated = len(updated_files)
    num_ignored = len(ignored_files)
    print(f'Checked {num_total} and updated {num_updated} build files, '
          f'{num_ignored} of which are ignored by git under {root}')
    if num_ignored:
        print(f'\nThe following {num_ignored} files were ignored by git and '
              'may need separate CLs in their respective repositories:')
        for filepath in ignored_files:
            print('  ' + os.path.relpath(filepath, start=root))


if __name__ == '__main__':
    main()
