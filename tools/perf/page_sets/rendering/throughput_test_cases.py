# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms

class ThroughputMetricStory(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.THROUGHPUT_TEST]

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('AnimationOnTap'):
      action_runner.PressKey(' ')
      action_runner.Wait(5)
      action_runner.PressKey(' ')


class MainZeroImplSixty(ThroughputMetricStory):
  BASE_NAME = 'main_0fps_impl_60fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-impl-animations-throughput.html')


class MainThirtyImplSixty(ThroughputMetricStory):
  BASE_NAME = 'main_30fps_impl_60fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-impl-animations-throughput.html#30')


class MainSixtyImplSixty(ThroughputMetricStory):
  BASE_NAME = 'main_60fps_impl_60fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-impl-animations-throughput.html#60')


class MainFifteenImplZero(ThroughputMetricStory):
  BASE_NAME = 'main_15fps_impl_0fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-animations-throughput.html#15')


class MainThirtyImplZero(ThroughputMetricStory):
  BASE_NAME = 'main_30fps_impl_0fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-animations-throughput.html#30')


class MainSixtyImplZero(ThroughputMetricStory):
  BASE_NAME = 'main_60fps_impl_0fps'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/throughput_test_cases/'
         'main-animations-throughput.html#60')

