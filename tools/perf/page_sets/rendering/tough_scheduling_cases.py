# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughSchedulingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_SCHEDULING]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughSchedulingPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class TouchDraggingPage(ToughSchedulingPage):

  """Why: Simple JS touch dragging."""

  BASE_NAME = 'simple_touch_drag'
  URL = 'file://../tough_scheduling_cases/simple_touch_drag.html'

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#card',
          use_touch=True,
          direction='up',
          speed_in_pixels_per_second=150,
          distance=400)


class SimpleTextPage(ToughSchedulingPage):

  """ Why: Simple scrolling baseline"""

  BASE_NAME = 'simple_text_page'
  URL = 'file://../tough_scheduling_cases/simple_text_page.html'


class TouchHandlerScrollingPage(ToughSchedulingPage):

  """ Why: Touch handler scrolling baseline"""

  BASE_NAME = 'touch_handler_scrolling'
  URL = 'file://../tough_scheduling_cases/touch_handler_scrolling.html'


class RafScrollingPage(ToughSchedulingPage):

  """ Why: requestAnimationFrame scrolling baseline"""

  BASE_NAME = 'raf'
  URL = 'file://../tough_scheduling_cases/raf.html'


class RafCanvasScrollingPage(ToughSchedulingPage):

  """  Why: Test canvas blocking behavior"""

  BASE_NAME = 'raf_canvas'
  URL = 'file://../tough_scheduling_cases/raf_canvas.html'


class RafAnimationScrollingPage(ToughSchedulingPage):

  """  Why: Test a requestAnimationFrame with concurrent CSS animation"""

  BASE_NAME = 'raf_animation'
  URL = 'file://../tough_scheduling_cases/raf_animation.html'


class RafTouchAnimationScrollingPage(ToughSchedulingPage):

  """  Why: Stress test for the scheduler"""

  BASE_NAME = 'raf_touch_animation'
  URL = 'file://../tough_scheduling_cases/raf_touch_animation.html'


class SynchronizedScrollOffsetPage(ToughSchedulingPage):

  """Why: For measuring the latency of scroll-synchronized effects."""

  BASE_NAME = 'sync_scroll_offset'
  URL = 'file://../tough_scheduling_cases/sync_scroll_offset.html'

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollBounceAction'):
      action_runner.ScrollBouncePage()


class SecondBatchJsPage(ToughSchedulingPage):

  """Why: For testing dynamically loading a large batch of Javascript and
          running a part of it in response to user input.
  """

  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # Do a dummy tap to warm up the synthetic tap code path.
    action_runner.TapElement(selector='div[id="spinner"]')
    # Begin the action immediately because we want the page to update smoothly
    # even while resources are being loaded.
    action_runner.WaitForJavaScriptCondition('window.__ready !== undefined')

    with action_runner.CreateGestureInteraction('LoadAction'):
      action_runner.ExecuteJavaScript('kickOffLoading()')
      action_runner.WaitForJavaScriptCondition('window.__ready')
      # Click one second after the resources have finished loading.
      action_runner.Wait(1)
      action_runner.TapElement(selector='input[id="run"]')
      # Wait for the test to complete.
      action_runner.WaitForJavaScriptCondition('window.__finished')


class SecondBatchLightJsPage(SecondBatchJsPage):
  BASE_NAME = 'second_batch_js_light'
  URL = 'file://../tough_scheduling_cases/second_batch_js.html?light'


class SecondBatchJsMediumPage(SecondBatchJsPage):
  BASE_NAME = 'second_batch_js_medium'
  URL = 'file://../tough_scheduling_cases/second_batch_js.html?medium'


class SecondBatchJsHeavyPage(SecondBatchJsPage):
  BASE_NAME = 'second_batch_js_heavy'
  URL = 'file://../tough_scheduling_cases/second_batch_js.html?heavy'
