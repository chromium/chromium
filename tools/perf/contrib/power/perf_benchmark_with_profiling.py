# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.perf_benchmark import PerfBenchmark
from telemetry.timeline.chrome_trace_config import ChromeTraceConfig


class PerfBenchmarkWithProfiling(PerfBenchmark):
  """A convenience super class for benchmarks that need Perfetto CPU profiling.

  This class automatically enables Perfetto profiling (callstack sampling) on
  benchmarks (when supported), and sets parameters related to such sampling
  automatically, based on (1) which browser the benchmark is being run on, and
  (2) any user-supplied options.
  """

  def __init__(self, *args, **kwargs):
    super(PerfBenchmarkWithProfiling, self).__init__(*args, **kwargs)
    # The browser selected for benchmarking.
    self._browser_package = None

  # You should, if needed, override the methods below.

  def GetSamplingFrequencyHz(self):
    """Override this to set the (int) Hz frequency for sampling callstacks."""
    return None

  def CustomizeSystemTraceConfig(self, system_trace_config):
    """Override this to modify `system_trace_config` (a SystemTraceConfig)."""

  # DO NOT OVERRIDE the methods below.

  def CustomizeOptions(self, finder_options, possible_browser=None):
    """DO NOT OVERRIDE this method in your benchmark subclass.

    Instead, override SetExtraBrowserOptions.
    """
    super(PerfBenchmarkWithProfiling,
          self).CustomizeOptions(finder_options, possible_browser)

    if finder_options is None or possible_browser is None:
      return

    # Only Android is currently supported.
    if not PerfBenchmark.IsAndroid(possible_browser):
      return

    try:
      self._browser_package = possible_browser.browser_package
    except AttributeError:
      # Not an Android browser.
      pass

  def CreateCoreTimelineBasedMeasurementOptions(self):
    """DO NOT OVERRIDE this method in your benchmark subclass.

    Instead, override CustomizeSystemTraceConfig.
    """
    options = super(PerfBenchmarkWithProfiling,
                    self).CreateCoreTimelineBasedMeasurementOptions()

    # Here, we implicitly assume that CreateCoreTimelineBasedMeasurementOptions
    # is called after CustomizeOptions.
    if self._browser_package is not None:
      options.config.enable_experimental_system_tracing = True
      options.config.system_trace_config.EnableProfiling(
          # Enable wildcard to sample all processes for the selected browser.
          "{}*".format(self._browser_package),
          self.GetSamplingFrequencyHz())
      # This adds metadata about Chrome processes to the profile.
      options.config.system_trace_config.EnableChrome(ChromeTraceConfig())

    self.CustomizeSystemTraceConfig(options.config.system_trace_config)
    return options
