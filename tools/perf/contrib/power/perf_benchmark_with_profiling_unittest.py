# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from contrib.power.perf_benchmark_with_profiling \
    import PerfBenchmarkWithProfiling
from telemetry.core import android_platform
from telemetry.internal.backends import android_browser_backend_settings
from telemetry.internal.backends.chrome import android_browser_finder
from telemetry.internal.browser.possible_browser import PossibleBrowser
from telemetry.internal.platform import android_platform_backend
from telemetry.testing import options_for_unittests


class PerfBenchmarkForTesting(PerfBenchmarkWithProfiling):
  def GetSamplingFrequencyHz(self):
    return 1234

  def CustomizeSystemTraceConfig(self, system_trace_config):
    system_trace_config.EnableFtraceCpu()


class PerfBenchmarkWithProfilingTest(unittest.TestCase):
  def setUp(self):
    self._finder_options = options_for_unittests.GetCopy()
    self._fake_platform = mock.Mock(spec=android_platform.AndroidPlatform)
    # pylint: disable=protected-access
    self._fake_platform._platform_backend = mock.Mock(
        spec=android_platform_backend.AndroidPlatformBackend)

  def testWithoutBrowser(self):
    benchmark = PerfBenchmarkForTesting()
    benchmark.CustomizeOptions(finder_options=self._finder_options,
                               possible_browser=None)
    options = benchmark.CreateCoreTimelineBasedMeasurementOptions()
    text_config = options.config.system_trace_config.GetTextConfig()
    self.assertNotIn('name: "linux.perf"', text_config)

  def testWithoutAndroidBrowser(self):
    benchmark = PerfBenchmarkForTesting()
    possible_browser = PossibleBrowser(browser_type="reference",
                                       target_os="linux",
                                       supports_tab_control=None)
    benchmark.CustomizeOptions(finder_options=self._finder_options,
                               possible_browser=possible_browser)
    options = benchmark.CreateCoreTimelineBasedMeasurementOptions()
    text_config = options.config.system_trace_config.GetTextConfig()
    self.assertNotIn('name: "linux.perf"', text_config)

  @unittest.skip("Temporary disabled to facilitate an API change in Telemetry.")
  def testWithAndroidBrowser(self):
    benchmark = PerfBenchmarkForTesting()
    possible_browser = android_browser_finder.PossibleAndroidBrowser(
        "android-chromium-bundle", self._finder_options, self._fake_platform,
        android_browser_backend_settings.ANDROID_CHROMIUM_BUNDLE)
    benchmark.CustomizeOptions(finder_options=self._finder_options,
                               possible_browser=possible_browser)
    options = benchmark.CreateCoreTimelineBasedMeasurementOptions()
    text_config = options.config.system_trace_config.GetTextConfig()
    self.assertIn('name: "linux.perf"', text_config)
    self.assertIn('sampling_frequency: 1234', text_config)
    self.assertIn('target_cmdline: "org.chromium.chrome*"', text_config)
    self.assertIn('name: "org.chromium.trace_event"', text_config)
    self.assertIn('name: "org.chromium.trace_metadata"', text_config)
