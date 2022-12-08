# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms
import contrib.power.stories as stories
from contrib.power.power_perf_benchmark_base import PowerPerfBenchmarkBase
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerWakeups(PowerPerfBenchmarkBase):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    del options  # unused
    return stories.GetAllMobileSystemHealthStories()

  def GetTraceConfig(self, browser_package):
    return """
      buffers: {
          size_kb: 1000000
          fill_policy: DISCARD
      }
      buffers: {
          size_kb: 2048
          fill_policy: DISCARD
      }
      data_sources {
          config {
              name: "linux.perf"
              perf_event_config {
                  timebase {
                      period: 1
                      tracepoint {
                          name: "sched/sched_waking"

                      }
                  }
                  callstack_sampling {
                      kernel_frames: true
                  }
              }
          }
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
                      \\"toplevel\\"
                    ],
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
                      \\"toplevel\\"
                    ],
                    \\"memory_dump_config\\": {}
                  }"
                  client_priority: USER_INITIATED
              }
          }
      }
      data_sources: {
          config {
              name: "linux.sys_stats"
              sys_stats_config {
                  stat_period_ms: 1000
                  stat_counters: STAT_CPU_TIMES
                  stat_counters: STAT_FORK_COUNT
              }
          }
      }
      data_sources: {
          config {
              name: "linux.ftrace"
              ftrace_config {
                  ftrace_events: "sched/sched_switch"
                  ftrace_events: "power/suspend_resume"
                  ftrace_events: "sched/sched_wakeup"
                  ftrace_events: "sched/sched_wakeup_new"
                  ftrace_events: "sched/sched_waking"
                  ftrace_events: "power/cpu_frequency"
                  ftrace_events: "power/cpu_idle"
                  ftrace_events: "regulator/regulator_set_voltage"
                  ftrace_events: "regulator/regulator_set_voltage_complete"
                  ftrace_events: "power/clock_enable"
                  ftrace_events: "power/clock_disable"
                  ftrace_events: "power/clock_set_rate"
                  ftrace_events: "sched/sched_process_exit"
                  ftrace_events: "sched/sched_process_free"
                  ftrace_events: "task/task_newtask"
                  ftrace_events: "task/task_rename"
                  buffer_size_kb: 131072
              }
          }
      }
      write_into_file: true
    """

  @classmethod
  def Name(cls):
    return 'contrib.power.wakeups'
