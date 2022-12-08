# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms
import contrib.power.stories as stories
from contrib.power.power_perf_benchmark_base import PowerPerfBenchmarkBase
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerIpc(PowerPerfBenchmarkBase):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    del options  # unused
    return stories.GetAllMobileSystemHealthStories()

  def GetTraceConfig(self, browser_package):
    return """
      buffers: {
          size_kb: 262144
          fill_policy: DISCARD
      }
      buffers: {
          size_kb: 2048
          fill_policy: DISCARD
      }
      data_sources: {
          config {
              name: "linux.process_stats"
              target_buffer: 1
              process_stats_config {
                  scan_all_processes_on_start: true
              }
          }
      }
      data_sources: {
          config {
              name: "org.chromium.trace_event"
              chrome_config {
                  trace_config: "{
                    \\"record_mode\\": \\"record-until-full\\",
                    \\"included_categories\\": [
                      \\"toplevel.flow\\",
                      \\"toplevel\\",
                      \\"mojom\\",
                      \\"navigation\\",
                      \\"ipc\\"
                    ],
                    \\"excluded_categories\\": [\\"*\\"],
                    \\"memory_dump_config\\": {}
                  }"
                  client_priority: USER_INITIATED
              }
          }
      }
      data_sources: {
          config {
              name: "org.chromium.trace_metadata"
              chrome_config {
                  trace_config: "{
                    \\"record_mode\\": \\"record-until-full\\",
                    \\"included_categories\\": [
                      \\"toplevel.flow\\",
                      \\"toplevel\\",
                      \\"mojom\\",
                      \\"navigation\\",
                      \\"ipc\\"
                    ],
                    \\"excluded_categories\\": [\\"*\\"],
                    \\"memory_dump_config\\": {}
                  }"
                  client_priority: USER_INITIATED
              }
          }
      }
      write_into_file: true
    """

  @classmethod
  def Name(cls):
    return 'contrib.power.ipc'
