# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys


def CreateInput(test_suite, platforms, metrics, test_cases_path, output_dir):
  with open(test_cases_path, 'r') as test_case_file:
    test_cases = [line.strip() for line in test_case_file]
  json_data = []

  for platform in platforms:
    for metric in metrics:
      for test_case in test_cases:
        json_data.append({
          'test_suite': test_suite,
          'bot': platform,
          'measurement': metric,
          'test_case': test_case
        })

  with open(output_dir, 'w') as output:
    json.dump(json_data, output)


def Main(argv):
  parser = argparse.ArgumentParser(
    description=('Creates the Json needed for the soundwave'))
  parser.add_argument('test_suite', help=('Name of test_suite (example: "'
            'rendering.desktop")'))
  parser.add_argument('--platforms', help='Name of platform (example: '
            '"ChromiumPerf:Win 7 Nvidia GPU Perf")', nargs='*')
  parser.add_argument('--metrics', help='Name of measurement (example: '
            '"frame_times")', nargs='*')
  parser.add_argument('--test-cases-path', type=str,
            help='Path for the file having test_cases')
  parser.add_argument('--output-dir', type=str,
            help='Path for the output file')

  args = parser.parse_args(argv[1:])
  return CreateInput(
    args.test_suite,
    args.platforms,
    args.metrics,
    args.test_cases_path,
    args.output_dir)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
