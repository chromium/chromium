# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_based_measurement


class DualMetricMeasurement(story_test.StoryTest):
  """Test class supporting both ad hoc measurements and trace based metrics.

    Currently only works with PressStory pages, which implement
    GetMeasurements().
  """
  def __init__(self, tbm_options):
    super(DualMetricMeasurement, self).__init__()
    # Only enable tracing if metrics have been specified.
    if tbm_options.GetTimelineBasedMetrics():
      self._tbm_test = timeline_based_measurement.TimelineBasedMeasurement(
          tbm_options)
      self._enable_tracing = True
    else:
      self._enable_tracing = False

  def WillRunStory(self, platform, story=None):
    if self._enable_tracing:
      self._tbm_test.WillRunStory(platform, story)

  def Measure(self, platform, results):
    if self._enable_tracing:
      self._tbm_test.Measure(platform, results)
    else:
      for value in results.current_story.GetMeasurements():
        results.AddMeasurement(**value)

  def DidRunStory(self, platform, results):
    if self._enable_tracing:
      self._tbm_test.DidRunStory(platform, results)
