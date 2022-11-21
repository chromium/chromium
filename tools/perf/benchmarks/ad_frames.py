# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
import page_sets

from benchmarks import loading_metrics_category
from telemetry import benchmark
from telemetry.web_perf import timeline_based_measurement


class _AdFramesBase(perf_benchmark.PerfBenchmark):
  """ A base class for loading benchmarks into ad frames. """

  def SetExtraBrowserOptions(self, options):
    # --disable-background-networking causes the server to send the ads without
    # the `supports-loading-mode: fenced-frames` header. So we allow background
    # networking so the headers allow the page to load in a fenced frame.
    options.disable_background_networking = False

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    loading_metrics_category.AugmentOptionsForLoadingMetrics(options)
    # Enable "Memory.GPU.PeakMemoryUsage2.PageLoad" so we can measure the GPU
    # memory used throughout the page loading tests. Include "umaMetric" as a
    # timeline so that we can parse this UMA Histogram.
    options.config.chrome_trace_config.EnableUMAHistograms(
        'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
        'PageLoad.Clients.FencedFrames.PaintTiming.NavigationToFirstContentfulPaint',
        'PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3')

    # Add "umaMetric" to the timeline based metrics. This does not override
    # those added in loading_metrics_category.AugmentOptionsForLoadingMetrics.
    options.AddTimelineBasedMetric('umaMetric')
    options.AddTimelineBasedMetric('loadingMetric')

    return options


@benchmark.Info(emails=['lbrady@google.com'],
                component='Blink>FencedFrames',
                documentation_url='https://tinyurl.com/fenced-frame-benchmark')
class FencedFrameBenchmark(_AdFramesBase):
  """ A benchmark measuring loading performance inside MPArch fenced frames. """

  def CreateStorySet(self, options):
    return page_sets.FencedFramePageSet()

  @classmethod
  def Name(cls):
    return 'ad_frames.fencedframe'


@benchmark.Info(emails=['lbrady@google.com'],
                component='Blink>FencedFrames',
                documentation_url='https://tinyurl.com/fenced-frame-benchmark')
class IframeBenchmark(_AdFramesBase):
  """ A benchmark measuring loading performance inside iframes. """

  def CreateStorySet(self, options):
    return page_sets.IframePageSet()

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_ad_frames.iframe'
