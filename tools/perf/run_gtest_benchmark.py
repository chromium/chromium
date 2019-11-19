#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs gtest perf tests and process results.

Runs gtest and processes traces (run metrics) to produce perf results.
"""

import argparse
import os
import shutil
import sys

from core import path_util
path_util.AddPyUtilsToPath()
sys.path.append(path_util.GetTracingDir())

from core import results_processor

sys.path.append(os.path.join(path_util.GetChromiumSrcDir(), 'testing'))
import test_env


# Note the name should be the one used by results_processor.ProcessResults.
MERGED_RESULTS = '_test_results.jsonl'

def _GetTraceDir(options):
  return os.path.join(options.intermediate_dir, 'trace')


def RunGTest(options, gtest_args):
  """Runs gtest with --trace-dir switch pointing at intermediate dir.

  Args:
    options: Parsed command line options.
    gtest_args: List of args to run gtest.

  Returns gtest run return code.
  """
  trace_dir = _GetTraceDir(options)
  os.makedirs(trace_dir)
  gtest_args.append('--trace-dir=%s' % trace_dir)

  gtest_command = [options.executable]
  gtest_command.extend(gtest_args)

  return_code = test_env.run_command(gtest_command)
  return return_code


def _MergeResultsJson(trace_dir, output_file):
  """Merge results json files generated in each test case into output_file."""
  result_files = [
      os.path.join(trace_dir, trace)
      for trace in os.listdir(trace_dir)
      if trace.endswith('test_result.json')
  ]
  with open(output_file, 'w') as output:
    for result_file in result_files:
      with open(result_file) as f:
        stripped_lines = [line.rstrip() for line in f]
        for line in stripped_lines:
          output.write('%s\n' % line)


def ProcessResults(options):
  """Collect generated results and call results_processor to compute results."""
  _MergeResultsJson(_GetTraceDir(options),
                    os.path.join(options.intermediate_dir, MERGED_RESULTS))
  process_return_code = results_processor.ProcessResults(options)
  if process_return_code != 0:
    return process_return_code
  expected_perf_filename = os.path.join(options.output_dir, 'histograms.json')
  output_perf_results = os.path.join(options.output_dir, 'perf_results.json')
  shutil.move(expected_perf_filename, output_perf_results)
  return process_return_code


def main(args):
  parser = argparse.ArgumentParser(parents=[results_processor.ArgumentParser()])
  parser.add_argument('executable', help='The name of the executable to run.')

  options, leftover_args = parser.parse_known_args(args)
  options.test_path_format = 'gtest'
  results_processor.ProcessOptions(options)

  run_return_code = RunGTest(options, leftover_args)
  process_return_code = ProcessResults(options)
  if process_return_code != 0:
    return process_return_code
  else:
    return run_return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
