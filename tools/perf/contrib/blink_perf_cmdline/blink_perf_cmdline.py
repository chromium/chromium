# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os

from benchmarks import blink_perf


# pylint: disable=protected-access
class BlinkPerfAll(blink_perf._BlinkPerfBenchmark):

  @classmethod
  def Name(cls):
    return 'blink_perf'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--test-path',
                        default=blink_perf.BLINK_PERF_BASE_DIR,
                        help=('Path to blink perf tests. Could be an absolute '
                              'path, a relative path with respect to your '
                              'current directory or a relative path with '
                              'respect to third_party/blink/perf_tests)'))

  def CreateStorySet(self, options):
    if os.path.exists(options.test_path):
      path = os.path.abspath(options.test_path)
    else:
      path = os.path.join(blink_perf.BLINK_PERF_BASE_DIR, options.test_path)
    print()
    print('Running all tests in %s' % path)
    return blink_perf.CreateStorySetFromPath(path, blink_perf.SKIPPED_FILE)
