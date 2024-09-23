# Copyright 2019 The Chromium Authors
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


class _JetStream2Base(press._PressBenchmark):  # pylint:disable=protected-access
  """JetStream2, a combination of JavaScript and Web Assembly benchmarks.

  Run all the JetStream 2 benchmarks by default.
  """
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--test-list',
                        help='Only run specific tests, separated by commas.')


@benchmark.Info(
    emails=['vahl@chromium.org', 'cbruni@chromium.org'],
    component='Blink>JavaScript',
    documentation_url='https://browserbench.org/JetStream2.0/in-depth.html')
class JetStream20(_JetStream2Base):
  """JetStream 2.0"""
  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_jetstream20'

  def CreateStorySet(self, options):
    return page_sets.JetStream20StorySet(options.test_list)


@benchmark.Info(
    emails=['vahl@chromium.org', 'cbruni@chromium.org'],
    component='Blink>JavaScript',
    documentation_url='https://browserbench.org/JetStream2.1/in-depth.html')
class JetStream21(_JetStream2Base):
  """JetStream 2.1"""
  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_jetstream21'

  def CreateStorySet(self, options):
    return page_sets.JetStream21StorySet(options.test_list)


@benchmark.Info(
    emails=['vahl@chromium.org', 'cbruni@chromium.org'],
    component='Blink>JavaScript',
    documentation_url='https://browserbench.org/JetStream2.0/in-depth.html')
class JetStream2(_JetStream2Base):
  """Latest JetStream2 """
  @classmethod
  def Name(cls):
    return 'jetstream2'

  def CreateStorySet(self, options):
    return page_sets.JetStream2StorySet(options.test_list)


@benchmark.Info(
    emails=['omerkatz@chromium.org'],
    component='Blink>JavaScript>GarbageCollection',
    documentation_url='https://browserbench.org/JetStream2.0/in-depth.html')
class JetStream2MinorMS(JetStream2):
  """Latest JetStream2 without the MinorMS flag.

  Shows the performance with Scavenger young generation GC in V8.
  """
  @classmethod
  def Name(cls):
    return 'jetstream2-minorms'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--js-flags=--minor-ms')
