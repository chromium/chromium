# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Octane 2.0 javascript benchmark.

Octane 2.0 is a modern benchmark that measures a JavaScript engine's performance
by running a suite of tests representative of today's complex and demanding web
applications. Octane's goal is to measure the performance of JavaScript code
found in large, real-world web applications.
Octane 2.0 consists of 17 tests, four more than Octane v1.
"""

from telemetry import benchmark
import page_sets
from benchmarks import press


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink>JavaScript')
class Octane(press._PressBenchmark): # pylint: disable=protected-access
  """Google's Octane JavaScript benchmark.

  http://chromium.github.io/octane/index.html?auto=1
  """
  @classmethod
  def Name(cls):
    return 'octane'

  def CreateStorySet(self, options):
    return page_sets.OctaneStorySet()
