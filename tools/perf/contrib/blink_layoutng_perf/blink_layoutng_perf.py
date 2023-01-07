# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import blink_perf
from telemetry import benchmark

# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfLayoutNg(blink_perf._BlinkPerfBenchmark):
  SUBDIR = 'layout'

  def SetExtraBrowserOptions(self, options):
    super(BlinkPerfLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'blink_perf.layout_ng'

# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfParserLayoutNg(blink_perf._BlinkPerfBenchmark):
  SUBDIR = 'parser'

  def SetExtraBrowserOptions(self, options):
    super(BlinkPerfParserLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'blink_perf.parser_layout_ng'

# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfPaintLayoutNg(blink_perf._BlinkPerfBenchmark):
  SUBDIR = 'paint'

  def SetExtraBrowserOptions(self, options):
    super(BlinkPerfPaintLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'blink_perf.paint_layout_ng'
