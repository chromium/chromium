#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict

import argparse
import csv
import os
import pathlib


def main():
    parser = argparse.ArgumentParser(description=(
        'Lists filtered tests in .filter files in testing/buildbot/filters,'
        ' outputs a .csv'))
    parser.add_argument('-o',
                        '--output-file',
                        type=pathlib.Path,
                        required=True,
                        help='output CSV file path')
    args = parser.parse_args()

    filter_dir = pathlib.Path('testing/buildbot/filters')
    filter_file_paths = [
        f for f in filter_dir.iterdir()
        if f.is_file() and f.name.endswith('_apk.filter')
    ]

    filtered_tests_by_builder = {}
    for filter_file_path in filter_file_paths:
        filtered_tests = []
        with open(filter_file_path, 'r') as input_filter_file:
            for line in input_filter_file.readlines():
                stripped_line = line.strip()
                if stripped_line.startswith('-'):
                    filtered_tests.append(stripped_line[1:])
        builder = filter_file_path.name.split('.')[1]
        filtered_tests_by_builder[builder] = filtered_tests

    all_filtered_tests_to_builders = defaultdict(list)
    for filter_file_name, filtered_tests in filtered_tests_by_builder.items():
        for filtered_test in filtered_tests:
            all_filtered_tests_to_builders[filtered_test].append(
                filter_file_name)

    with open(args.output_file, 'w') as csv_output_file:
        csv_writer = csv.writer(csv_output_file)

        for filtered_test in sorted(all_filtered_tests_to_builders):
            builders = all_filtered_tests_to_builders[filtered_test]
            csv_writer.writerow(
                ['.filter', filtered_test, ', '.join(builders)])


if __name__ == '__main__':
    main()
