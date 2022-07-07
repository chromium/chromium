# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import unittest

import mock

from core.perf_benchmark_with_profiling import PerfBenchmarkWithProfiling
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

  def assertEqualIgnoringWhitespace(self, actual, expected):

    def removeWhitespace(text):
      return re.sub(r"\s+", "", text, flags=re.UNICODE)

    return self.assertEqual(removeWhitespace(actual),
                            removeWhitespace(expected))

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
    self.assertEqualIgnoringWhitespace(
        options.config.system_trace_config.GetTextConfig(), """
        buffers: {
          size_kb: 200000
          fill_policy: DISCARD
        }
        duration_ms: 1800000
        data_sources: {
          config {
            name: "linux.ftrace"
            ftrace_config {
              ftrace_events: "power/suspend_resume"
              ftrace_events: "power/cpu_frequency"
              ftrace_events: "power/cpu_idle"
            }
          }
        }""")
    self.assertNotIn("PERFETTO_SYMBOLIZER_MODE", os.environ)
    self.assertNotIn("PERFETTO_BINARY_PATH", os.environ)

  def testWithoutAndroidBrowser(self):
    benchmark = PerfBenchmarkForTesting()
    possible_browser = PossibleBrowser(browser_type="reference",
                                       target_os="linux",
                                       supports_tab_control=None)
    benchmark.CustomizeOptions(finder_options=self._finder_options,
                               possible_browser=possible_browser)
    options = benchmark.CreateCoreTimelineBasedMeasurementOptions()
    self.assertEqualIgnoringWhitespace(
        options.config.system_trace_config.GetTextConfig(), """
        buffers: {
          size_kb: 200000
          fill_policy: DISCARD
        }
        duration_ms: 1800000
        data_sources: {
          config {
            name: "linux.ftrace"
            ftrace_config {
              ftrace_events: "power/suspend_resume"
              ftrace_events: "power/cpu_frequency"
              ftrace_events: "power/cpu_idle"
            }
          }
        }""")
    self.assertNotIn("PERFETTO_SYMBOLIZER_MODE", os.environ)
    self.assertNotIn("PERFETTO_BINARY_PATH", os.environ)

  def testWithAndroidBrowser(self):
    benchmark = PerfBenchmarkForTesting()
    possible_browser = android_browser_finder.PossibleAndroidBrowser(
        "android-chromium-bundle", self._finder_options, self._fake_platform,
        android_browser_backend_settings.ANDROID_CHROMIUM_BUNDLE)
    benchmark.CustomizeOptions(finder_options=self._finder_options,
                               possible_browser=possible_browser)
    options = benchmark.CreateCoreTimelineBasedMeasurementOptions()
    self.assertEqualIgnoringWhitespace(
        options.config.system_trace_config.GetTextConfig(), """
        buffers: {
          size_kb: 200000
          fill_policy: DISCARD
        }
        buffers {
          size_kb: 190464
        }

        buffers {
          size_kb: 190464
        }
        data_sources {
          config {
            name: "linux.process_stats"
            target_buffer: 1
            process_stats_config {
                scan_all_processes_on_start: true
                record_thread_names: true
                proc_stats_poll_ms: 100
            }
          }
        }
        data_sources {
          config {
            name: "linux.perf"
            target_buffer: 2
            perf_event_config {
                all_cpus: true
                sampling_frequency: 1234
                target_cmdline: "org.chromium.chrome*"
            }
          }
        }
        duration_ms: 1800000
        data_sources: {
          config {
            name: "linux.ftrace"
            ftrace_config {
              ftrace_events: "power/suspend_resume"
              ftrace_events: "power/cpu_frequency"
              ftrace_events: "power/cpu_idle"
            }
          }
        }""")
    self.assertIn("PERFETTO_SYMBOLIZER_MODE", os.environ)
    self.assertIn("PERFETTO_BINARY_PATH", os.environ)
    self.assertTrue(
        os.environ["PERFETTO_BINARY_PATH"].endswith("lib.unstripped"))
