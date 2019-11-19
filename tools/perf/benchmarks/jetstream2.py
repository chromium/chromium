# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Apple's JetStream 2 benchmark.

JetStream 2 combines together a variety of JavaScript and Web Assembly
benchmarks, covering a variety of advanced workloads and programming
techniques, and reports a single score that balances them using a geometric
mean.

Each benchmark measures a distinct workload, and no single optimization
technique is sufficient to speed up all benchmarks. Some benchmarks
demonstrate tradeoffs, and aggressive or specialized optimizations for one
benchmark might make another benchmark slower. JetStream 2 rewards browsers
that start up quickly, execute code quickly, and continue running smoothly.

Each benchmark in JetStream 2 computes its own individual score. JetStream 2
weighs each benchmark equally, taking the geometric mean over each individual
benchmark's score to compute the overall JetStream 2 score.

"""

from telemetry import benchmark

import page_sets
from benchmarks import press


@benchmark.Info(emails=['hablich@chromium.org', 'tcwang@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://browserbench.org/JetStream/in-depth.html')

class Jetstream2(press._PressBenchmark): # pylint: disable=protected-access
  """JetStream2, a combination of JavaScript and Web Assembly benchmarks.

  Run all the Jetstream 2 benchmarks by default.
  """
  @classmethod
  def Name(cls):
    return 'jetstream2'

  def CreateStorySet(self, options):
    return page_sets.Jetstream2StorySet()
