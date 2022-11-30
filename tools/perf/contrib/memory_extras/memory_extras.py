# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import py_utils

from benchmarks import memory
from core import perf_benchmark

from telemetry import benchmark
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story as story_module


_DUMP_WAIT_TIME = 3
_ITERATIONS = 10


class DesktopMemorySharedState(shared_page_state.SharedDesktopPageState):
  def ShouldReuseBrowserForAllStoryRuns(self):
    return True


class DesktopMemoryPage(page_module.Page):

  def __init__(self, url, page_set):
    super(DesktopMemoryPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=DesktopMemorySharedState,
        name=url)

  def _DumpMemory(self, action_runner, phase):
    with action_runner.CreateInteraction(phase):
      action_runner.Wait(_DUMP_WAIT_TIME)
      action_runner.ForceGarbageCollection()
      action_runner.SimulateMemoryPressureNotification('critical')
      action_runner.Wait(_DUMP_WAIT_TIME)
      action_runner.tab.browser.DumpMemory()

  def RunPageInteractions(self, action_runner):
    self._DumpMemory(action_runner, 'pre')
    for _ in range(_ITERATIONS):
      action_runner.ReloadPage()

    tabs = action_runner.tab.browser.tabs
    for _ in range(_ITERATIONS):
      new_tab = tabs.New()
      new_tab.action_runner.Navigate(self._url)
      try:
        new_tab.action_runner.WaitForNetworkQuiescence()
      except py_utils.TimeoutException:
        logging.warning('WaitForNetworkQuiescence() timeout')
      new_tab.Close()

    self._DumpMemory(action_runner, 'post')


class DesktopMemoryPageSet(story_module.StorySet):
  """ Desktop sites with interesting memory characteristics """

  def __init__(self):
    super(DesktopMemoryPageSet, self).__init__()

    urls_list = [
      'http://www.google.com',
      "http://www.live.com",
      "http://www.youtube.com",
      "http://www.wikipedia.org",
      "http://www.flickr.com/",
      "http://www.cnn.com/",
      "http://www.adobe.com/",
      "http://www.aol.com/",
      "http://www.cnet.com/",
      "http://www.godaddy.com/",
      "http://www.walmart.com/",
      "http://www.skype.com/",
    ]

    for url in urls_list:
      self.AddStory(DesktopMemoryPage(url, self))


@benchmark.Info(emails=['etienneb@chromium.org'])
class LongRunningMemoryBenchmarkSitesDesktop(perf_benchmark.PerfBenchmark):
  """Measure memory usage on popular sites.

  This benchmark is intended to run locally over a long period of time. The
  data collected by this benchmark are not metrics but traces with memory dumps.
  The browser process is staying alive for the whole execution and memory dumps
  in these traces can be compare (diff) to determine which objects are potential
  memory leaks.
  """
  options = {
    'pageset_repeat': 30,
    'use_live_sites': True,
  }

  def CreateStorySet(self, options):
    return DesktopMemoryPageSet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    return memory.CreateCoreTimelineBasedMemoryMeasurementOptions()

  def SetExtraBrowserOptions(self, options):
    memory.SetExtraBrowserOptionsForMemoryMeasurement(options)
    options.AppendExtraBrowserArgs([
        '--memlog=all', '--memlog-sampling',
        '--memlog-stack-mode=native-with-thread-names'])
    # Disable taking screenshot on failing pages.
    options.take_screenshot_for_failed_page = False

  @classmethod
  def Name(cls):
    return 'memory.long_running_desktop_sites'
