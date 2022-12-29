#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Get test health information for a Git repository.

Example Usage:

tools/android/test_health/get_test_health.py \
  --output-file ~/test_data.jsonl \
  --git-dir ~/git/chromium/src \
  --test-dir chrome/browser/android
"""

import argparse
import logging
import pathlib
import time

import test_health_exporter
import test_health_extractor


def main():
    parser = argparse.ArgumentParser(
        description='Gather Java test health information for a Git repository'
        ' and export it as newline-delimited JSON.')
    parser.add_argument('-o',
                        '--output-file',
                        type=pathlib.Path,
                        required=True,
                        help='output file path for extracted test health data')
    parser.add_argument('--git-dir',
                        type=pathlib.Path,
                        required=False,
                        help='root directory of the Git repository to read'
                        ' (defaults to the Chromium repo)')
    parser.add_argument('--test-dir',
                        type=pathlib.Path,
                        required=False,
                        help='subdirectory containing the tests of interest;'
                        ' defaults to the root of the Git repo')
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Used to display detailed logging.')
    args = parser.parse_args()

    if args.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    logging.info('Extracting test health data from Git repo.')
    start_time = time.time()
    test_health_list = test_health_extractor.get_repo_test_health(
        args.git_dir, test_dir=args.test_dir)
    extraction_time = time.time() - start_time
    logging.debug(f'--- Extraction took {extraction_time:.2f} seconds ---')

    logging.info('Exporting test health data to file: ' +
                 str(args.output_file))
    export_start_time = time.time()
    test_health_exporter.to_json_file(test_health_list, args.output_file)
    export_time = time.time() - export_start_time
    logging.debug(f'--- Export took {export_time:.2f} seconds ---')

    total_time = time.time() - start_time
    logging.debug(f'--- Took {total_time:.2f} seconds ---')


if __name__ == '__main__':
    main()
