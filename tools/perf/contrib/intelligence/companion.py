# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
import page_sets


@benchmark.Info(
    emails=['mgeorgaklis@google.com'],
    component='UI>Browser',
    documentation_url=
    'https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/side_panel/companion/'
)
class Companion(perf_benchmark.PerfBenchmark):
  """Companion Benchmark."""
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.CompanionStorySet()

  def SetExtraBrowserOptions(self, options):
    # Make sure finch experiment is turned off for benchmarking.
    options.AppendExtraBrowserArgs('--enable-benchmarking')
    # UIDevtools is used for driving native UI.
    options.AppendExtraBrowserArgs('--enable-ui-devtools=0')
    # Enable companion-specific logging.
    options.AppendExtraBrowserArgs('--enable-stats-collection-bindings')
    options.AppendExtraBrowserArgs(
        '--enable-features=ui-debug-tools-enable-synthetic-events,SidePanelCompanion'
    )
    options.AppendExtraBrowserArgs(
        '--disable-checking-companion-user-permissions')
    options.AppendExtraBrowserArgs('--disable-features=SideSearch')

  @classmethod
  def Name(cls):
    return 'contrib.companion'
