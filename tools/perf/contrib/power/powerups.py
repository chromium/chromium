# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms
import contrib.power.stories as stories
from contrib.power.power_perf_benchmark_base import PowerPerfBenchmarkBase
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerPowerups(PowerPerfBenchmarkBase):
  """Capture traces for analysis of CPU power usage patterns.

  This benchmark is intended to be run locally.  The data collected by
  the benchmark are traces meant for subsequent analysis using Perfetto
  scripts, not metrics.

  This benchmark runs the stories in the Mobile System Health story set,
  capturing kernel scheduling information and CPU power state transitions
  during the benchmark run.  The resulting trace can then be analyzed by
  Perfetto scripts such as "tools/perf/contrib/power/cpu_powerups.sql".
  """

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
      buffers {   # Buffer 2, 6MB for ftrace events.
          size_kb: 6144
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
      data_sources: {
          config {
              name: "linux.ftrace"
              target_buffer: 2
              ftrace_config {
                  compact_sched {
                      enabled: true
                  }
                  ftrace_events: "power/cpu_frequency"
                  ftrace_events: "power/cpu_idle"
                  ftrace_events: "sched/sched_switch"
                  ftrace_events: "sched/sched_process_blocked_reason"
                  ftrace_events: "sched/sched_process_exit"
                  ftrace_events: "sched/sched_process_free"
                  ftrace_events: "task/task_newtask"
                  ftrace_events: "task/task_rename"
              }
          }
      }
      write_into_file: true
    """

  @classmethod
  def Name(cls):
    return 'contrib.power.powerups'
