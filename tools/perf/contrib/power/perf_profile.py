# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contrib.power.stories as stories
from contrib.power.power_perf_benchmark_base import PowerPerfBenchmarkBase
from core import platforms
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerPerfProfile(PowerPerfBenchmarkBase):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    del options  # unused
    return stories.GetAllMobileSystemHealthStories()

  def GetTraceConfig(self, browser_package):
    return """
      buffers: {{
          size_kb: 500000
          fill_policy: DISCARD
      }}
      buffers: {{
          size_kb: 10000
          fill_policy: DISCARD
      }}
      data_sources {{
          config {{
              name: "linux.perf"
              perf_event_config {{
                  timebase {{
                      frequency: 300
                  }}
                  callstack_sampling {{
                    scope {{
                      target_cmdline: "{browser_package}*"
                    }}
                    kernel_frames: true
                  }}
              }}
          }}
      }}
      data_sources: {{
          config {{
              name: "linux.process_stats"
              target_buffer: 1
              process_stats_config {{
                  scan_all_processes_on_start: true
              }}
          }}
      }}
      data_sources: {{
          config {{
              name: "org.chromium.trace_metadata"
              chrome_config {{
                  trace_config: "{{
                    \\"record_mode\\": \\"record-until-full\\",
                    \\"excluded_categories\\": [ \\"*\\" ]
                  }}"
                  client_priority: USER_INITIATED
              }}
          }}
      }}
      data_sources: {{
          config {{
              name: "org.chromium.trace_event"
              chrome_config {{
                  trace_config: "{{
                    \\"record_mode\\": \\"record-until-full\\",
                    \\"excluded_categories\\": [ \\"*\\" ]
                  }}"
                  client_priority: USER_INITIATED
              }}
          }}
      }}
      write_into_file: true
    """.format(browser_package=browser_package)

  @classmethod
  def Name(cls):
    return 'contrib.power.perf_profile'
