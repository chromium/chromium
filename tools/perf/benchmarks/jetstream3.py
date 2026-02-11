# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs JetStream 3 benchmark.

See http://browserbench.org/JetStream3
"""

from telemetry import benchmark

import page_sets
from benchmarks import press


class _JetStream3Base(press._PressBenchmark):  # pylint:disable=protected-access
  """JetStream3, a combination of JavaScript and Web Assembly benchmarks.

  Run all the JetStream 3.x benchmarks by default.
  """

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--test-list',
                        help='Only run specific tests, separated by commas.')


@benchmark.Info(
    emails=['vahl@chromium.org', 'cbruni@chromium.org'],
    component='Blink>JavaScript',
    documentation_url='https://browserbench.org/JetStream3.0/in-depth.html')
class JetStream30(_JetStream3Base):
  """JetStream 3.0 """

  SCHEDULED = False

  @classmethod
  def Name(cls):
    return 'jetstream30.telemetry'

  def CreateStorySet(self, options):
    return page_sets.JetStream30StorySet(options.test_list)


@benchmark.Info(
    emails=['vahl@chromium.org', 'cbruni@chromium.org'],
    component='Blink>JavaScript',
    documentation_url='https://browserbench.org/JetStream3.0/in-depth.html')
class JetStream3(_JetStream3Base):
  """Latest JetStream 3.x """

  SCHEDULED = False

  @classmethod
  def Name(cls):
    return 'jetstream3.telemetry'

  def CreateStorySet(self, options):
    return page_sets.JetStream3StorySet(options.test_list)
