#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a test repeatedly to measure its flakiness. The return code is non-zero
if the failure rate is higher than the specified threshold, but is not 100%."""

import argparse
import multiprocessing.dummy
import subprocess
import sys
import time

def load_options():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--retries', default=1000, type=int,
                      help='Number of test retries to measure flakiness.')
  parser.add_argument('--threshold', default=0.05, type=float,
                      help='Minimum flakiness level at which test is '
                           'considered flaky.')
  parser.add_argument('--jobs', '-j', type=int, default=1,
                      help='Number of parallel jobs to run tests.')
  parser.add_argument('command', nargs='+', help='Command to run test.')
  return parser.parse_args()

def run_test(job):
  print('Starting retry attempt %d out of %d' % (job['index'] + 1,
                                                 job['retries']))
  return subprocess.check_call(job['cmd'], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)

def main():
  options = load_options()
  num_passed = num_failed = 0
  running = []

  pool = multiprocessing.dummy.Pool(processes=options.jobs)
  args = [{'index': index, 'retries': options.retries, 'cmd': options.command}
          for index in range(options.retries)]
  results = pool.map(run_test, args)
  num_passed = len([retcode for retcode in results if retcode == 0])
  num_failed = len(results) - num_passed

  if num_passed == 0:
    flakiness = 0
  else:
    flakiness = num_failed / float(len(results))

  print('Flakiness is %.2f' % flakiness)
  if flakiness > options.threshold:
    return 1
  else:
    return 0


if __name__ == '__main__':
  sys.exit(main())
