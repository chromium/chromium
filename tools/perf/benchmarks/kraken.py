# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

from telemetry import benchmark

import page_sets
from benchmarks import press


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink>JavaScript')
class Kraken(press._PressBenchmark): # pylint: disable=protected-access
  """Mozilla's Kraken JavaScript benchmark.

  http://krakenbenchmark.mozilla.org/
  """
  @classmethod
  def Name(cls):
    return 'kraken'

  def CreateStorySet(self, options):
    return page_sets.KrakenStorySet()
