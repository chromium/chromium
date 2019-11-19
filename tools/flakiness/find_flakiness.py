#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains two functions that run different test cases and the same test
case in parallel repeatedly to identify flaky tests.
"""

from __future__ import print_function

import os
import re
import subprocess
import time


# Defaults for FindShardingFlakiness().
FF_DATA_SUFFIX = '_flakies'
FF_SLEEP_INTERVAL = 10.0
FF_NUM_ITERATIONS = 100
FF_SUPERVISOR_ARGS = ['-r3', '--random-seed']

# Defaults for FindUnaryFlakiness().
FF_OUTPUT_SUFFIX = '_purges'
FF_NUM_PROCS = 20
FF_NUM_REPEATS = 10
FF_TIMEOUT = 600


def FindShardingFlakiness(test_path, data_path, supervisor_args):
  """Finds flaky test cases by sharding and running a test for the specified
  number of times. The data file is read at the beginning of each run to find
  the last known counts and is overwritten at the end of each run with the new
  counts. There is an optional sleep interval between each run so the script can
  be killed without losing the data, useful for overnight (or weekend!) runs.
  """

  failed_tests = {}
  # Read a previously written data file.
  if os.path.exists(data_path):
    data_file = open(data_path, 'r')
    num_runs = int(data_file.readline().split(' ')[0])
    num_passes = int(data_file.readline().split(' ')[0])
    for line in data_file:
      if line:
        split_line = line.split(' -> ')
        failed_tests[split_line[0]] = int(split_line[1])
    data_file.close()
  # No data file found.
  else:
    num_runs = 0
    num_passes = 0

  log_lines = False
  args = ['python', '../sharding_supervisor/sharding_supervisor.py']
  args.extend(supervisor_args + [test_path])
  proc = subprocess.Popen(args, stderr=subprocess.PIPE)

  # Shard the test and collect failures.
  while True:
    line = proc.stderr.readline()
    if not line:
      if proc.poll() is not None:
        break
      continue
    print(line.rstrip())
    if log_lines:
      line = line.rstrip()
      if line in failed_tests:
        failed_tests[line] += 1
      else:
        failed_tests[line] = 1
    elif line.find('FAILED TESTS:') >= 0:
      log_lines = True
  num_runs += 1
  if proc.returncode == 0:
    num_passes += 1

  # Write the data file and print results.
  data_file = open(data_path, 'w')
  print('%i runs' % num_runs)
  data_file.write('%i runs\n' % num_runs)
  print('%i passes' % num_passes)
  data_file.write('%i passes\n' % num_passes)
  for (test, count) in failed_tests.iteritems():
    print('%s -> %i' % (test, count))
    data_file.write('%s -> %i\n' % (test, count))
  data_file.close()


def FindUnaryFlakiness(test_path, output_path, num_procs, num_repeats, timeout):
  """Runs all the test cases in a given test in parallel with itself, to get at
  those that hold on to shared resources. The idea is that if a test uses a
  unary resource, then running many instances of this test will purge out some
  of them as failures or timeouts.
  """

  test_name_regex = r'((\w+/)?\w+\.\w+(/\d+)?)'
  test_start = re.compile('\[\s+RUN\s+\] ' + test_name_regex)
  test_list = []

  # Run the test to discover all the test cases.
  proc = subprocess.Popen([test_path], stdout=subprocess.PIPE)
  while True:
    line = proc.stdout.readline()
    if not line:
      if proc.poll() is not None:
        break
      continue
    print(line.rstrip())
    results = test_start.search(line)
    if results:
      test_list.append(results.group(1))

  failures = []
  index = 0
  total = len(test_list)

  # Run each test case in parallel with itself.
  for test_name in test_list:
    num_fails = 0
    num_terminated = 0
    procs = []
    args = [test_path, '--gtest_filter=' + test_name,
            '--gtest_repeat=%i' % num_repeats]
    while len(procs) < num_procs:
      procs.append(subprocess.Popen(args))
    seconds = 0
    while procs:
      for proc in procs:
        if proc.poll() is not None:
          if proc.returncode != 0:
            ++num_fails
          procs.remove(proc)
      # Timeout exceeded, kill the remaining processes and make a note.
      if seconds > timeout:
        num_fails += len(procs)
        num_terminated = len(procs)
        while procs:
          procs.pop().terminate()
      time.sleep(1.0)
      seconds += 1
    if num_fails:
      line = '%s: %i failed' % (test_name, num_fails)
      if num_terminated:
        line += ' (%i terminated)' % num_terminated
      failures.append(line)
    print('%s (%i / %i): %i failed' % (test_name, index, total, num_fails))
    index += 1
    time.sleep(1.0)

  # Print the results and write the data file.
  print(failures)
  data_file = open(output_path, 'w')
  for line in failures:
    data_file.write(line + '\n')
  data_file.close()


def main():
  if not args:
    parser.error('You must specify a path to test!')
  if not os.path.exists(args[0]):
    parser.error('%s does not exist!' % args[0])

  data_path = os.path.basename(args[0]) + FF_DATA_SUFFIX
  output_path = os.path.basename(args[0]) + FF_OUTPUT_SUFFIX

  for i in range(FF_NUM_ITERATIONS):
    FindShardingFlakiness(args[0], data_path, FF_SUPERVISOR_ARGS)
    print('That was just iteration %i of %i.' % (i + 1, FF_NUM_ITERATIONS))
    time.sleep(FF_SLEEP_INTERVAL)

  FindUnaryFlakiness(
      args[0], output_path, FF_NUM_PROCS, FF_NUM_REPEATS, FF_TIMEOUT)


if __name__ == '__main__':
  main()
