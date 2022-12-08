# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.perf_benchmark import PerfBenchmark
from telemetry.web_perf import timeline_based_measurement


class PowerPerfBenchmarkBase(PerfBenchmark):
  def __init__(self, *args, **kwargs):
    super(PowerPerfBenchmarkBase, self).__init__(*args, **kwargs)
    # The browser selected for benchmarking.
    self._browser_package = None

  def GetTraceConfig(self, browser_package):
    raise NotImplementedError("GetTraceConfig not implemented")

  def CustomizeOptions(self, finder_options, possible_browser=None):
    super(PowerPerfBenchmarkBase,
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
    options = timeline_based_measurement.Options()
    options.config.enable_experimental_system_tracing = True
    options.config.system_trace_config.SetTextConfig(
        self.GetTraceConfig(self._browser_package))
    return options
