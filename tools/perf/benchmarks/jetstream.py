# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Apple's JetStream benchmark.

JetStream combines a variety of JavaScript benchmarks, covering a variety of
advanced workloads and programming techniques, and reports a single score that
balances them using geometric mean.

Each benchmark measures a distinct workload, and no single optimization
technique is sufficient to speed up all benchmarks. Latency tests measure that
a web application can start up quickly, ramp up to peak performance quickly,
and run smoothly without interruptions. Throughput tests measure the sustained
peak performance of a web application, ignoring ramp-up time and spikes in
smoothness. Some benchmarks demonstrate trade-offs, and aggressive or
specialized optimization for one benchmark might make another benchmark slower.
"""

from telemetry import benchmark

import page_sets
from benchmarks import press


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink>JavaScript')
class Jetstream(press._PressBenchmark): # pylint: disable=protected-access
  @classmethod
  def Name(cls):
    return 'jetstream'

  def CreateStorySet(self, options):
    return page_sets.JetstreamStorySet()
