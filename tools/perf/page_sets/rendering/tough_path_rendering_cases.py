# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughPathRenderingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_PATH_RENDERING]

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.Wait(10)


class GUIMarkVectorChartPage(ToughPathRenderingPage):
  BASE_NAME = 'guimark_vector_chart'
  URL = 'http://www.craftymind.com/factory/guimark2/HTML5ChartingTest.html'


class MotionMarkCanvasFillShapesPage(ToughPathRenderingPage):
  BASE_NAME = 'motion_mark_canvas_fill_shapes'
  # pylint: disable=line-too-long
  URL = 'http://rawgit.com/WebKit/webkit/master/PerformanceTests/MotionMark/developer.html?test-name=Fillshapes&test-interval=20&display=minimal&tiles=big&controller=fixed&frame-rate=50&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Canvassuite&complexity=1000'
  TAGS = ToughPathRenderingPage.TAGS + [story_tags.REPRESENTATIVE_MOBILE]


class MotionMarkCanvasStrokeShapesPage(ToughPathRenderingPage):
  BASE_NAME = 'motion_mark_canvas_stroke_shapes'
  # pylint: disable=line-too-long
  URL = 'http://rawgit.com/WebKit/webkit/master/PerformanceTests/MotionMark/developer.html?test-name=Strokeshapes&test-interval=20&display=minimal&tiles=big&controller=fixed&frame-rate=50&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Canvassuite&complexity=1000'


class ChalkboardPage(rendering_story.RenderingStory):
  BASE_NAME = 'ie_chalkboard'
  URL = 'https://testdrive-archive.azurewebsites.net/performance/chalkboard/'
  TAGS = [
    story_tags.TOUGH_PATH_RENDERING,
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.EvaluateJavaScript(
          'document.getElementById("StartButton").click()')
      action_runner.Wait(20)
