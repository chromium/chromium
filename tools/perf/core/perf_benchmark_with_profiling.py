# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core.perf_benchmark import PerfBenchmark
from telemetry.core import util


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
    # A build/symbols directory, if any, for the browser being benchmarked.
    self._symbols_directory = None
    # Clear environment variables that are conditionally set by this class.
    os.environ.pop("PERFETTO_SYMBOLIZER_MODE", None)
    os.environ.pop("PERFETTO_BINARY_PATH", None)

  # You should, if needed, override the methods below.

  def GetSamplingFrequencyHz(self):
    """Override this to set the (int) Hz frequency for sampling callstacks."""
    return None

  def CustomizeSystemTraceConfig(self, system_trace_config):
    """Override this to modify `system_trace_config` (a SystemTraceConfig)."""
    pass

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

    self._symbols_directory = next(
        util.GetBuildDirectories(finder_options.chrome_root), None)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    """DO NOT OVERRIDE this method in your benchmark subclass.

    Instead, override CustomizeSystemTraceConfig.
    """
    options = super(PerfBenchmarkWithProfiling,
                    self).CreateCoreTimelineBasedMeasurementOptions()

    if self._browser_package is not None:
      options.config.enable_experimental_system_tracing = True
      options.config.system_trace_config.EnableProfiling(
          # Enable wildcard to sample all processes for the selected browser.
          "{}*".format(self._browser_package),
          self.GetSamplingFrequencyHz())
      if self._symbols_directory is not None:
        os.environ["PERFETTO_SYMBOLIZER_MODE"] = "index"
        os.environ["PERFETTO_BINARY_PATH"] = self._symbols_directory

    self.CustomizeSystemTraceConfig(options.config.system_trace_config)
    return options
