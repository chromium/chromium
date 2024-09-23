# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.internal.actions import page_action
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughFastScrollingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  EXTRA_BROWSER_ARGS = None
  SELECTOR = None
  SPEED_IN_PIXELS_PER_SECOND = None
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_DEFAULT
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOUGH_SCROLLING]
  USE_FLING_SCROLL = False
  VSYNC_OFFSET_US = 0
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_DEFAULT

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    if self.EXTRA_BROWSER_ARGS is not None:
      if extra_browser_args is None:
        extra_browser_args = []
      extra_browser_args.append(self.EXTRA_BROWSER_ARGS)
    super(ToughFastScrollingPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    start = time.time()
    selector = self.SELECTOR
    with action_runner.CreateGestureInteraction('ScrollAction'):
      direction = 'up' if self.USE_FLING_SCROLL else 'down'
      # Some of the metrics the benchmark reports require the scroll to run for
      # a few seconds (5+). Therefore, scroll the page for long enough that
      # these metrics are accurately reported.
      while time.time() - start < 15:
        if self.USE_FLING_SCROLL:
          action_runner.SwipePage(
              direction=direction,
              speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND)
          action_runner.Wait(1)
        else:
          if selector is None:
            action_runner.ScrollPage(
                direction=direction,
                speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND,
                synthetic_gesture_source=self.SYNTHETIC_GESTURE_SOURCE,
                vsync_offset_ms=self.VSYNC_OFFSET_US * 0.001,
                input_event_pattern=self.INPUT_EVENT_PATTERN)
          else:
            # When there is a `selector` specified, scroll just that particular
            # element, rather than the entire page.
            action_runner.ScrollElement(
                selector=selector,
                direction=direction,
                speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND,
                synthetic_gesture_source=self.SYNTHETIC_GESTURE_SOURCE,
                vsync_offset_ms=self.VSYNC_OFFSET_US * 0.001,
                input_event_pattern=self.INPUT_EVENT_PATTERN)
        direction = 'up' if direction == 'down' else 'down'


class FlingingText05000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_fling_05000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  USE_FLING_SCROLL = True
  SPEED_IN_PIXELS_PER_SECOND = 5000


class FlingingText10000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_fling_10000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  USE_FLING_SCROLL = True
  SPEED_IN_PIXELS_PER_SECOND = 10000


class FlingingText20000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_fling_20000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  USE_FLING_SCROLL = True
  SPEED_IN_PIXELS_PER_SECOND = 20000


class ScrollingText5000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_05000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 5000


class ScrollingText10000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_10000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 10000
  TAGS = ToughFastScrollingPage.TAGS + [story_tags.REPRESENTATIVE_MOBILE]


class ScrollingText20000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_20000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 20000


class ScrollingText40000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_40000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 40000


class ScrollingText60000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_60000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 60000


class ScrollingText75000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_75000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 75000


class ScrollingText90000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_90000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 90000


class ScrollingTextHover5000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_05000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 5000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover10000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_10000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 10000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover20000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_20000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 20000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover40000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_40000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 40000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover60000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_60000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 60000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover75000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_75000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 75000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextHover90000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_hover_90000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_hover.html'
  SPEED_IN_PIXELS_PER_SECOND = 90000
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE


class ScrollingTextRaster5000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_05000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 5000


class ScrollingTextRaster10000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_10000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 10000


class ScrollingTextRaster20000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_20000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 20000


class ScrollingTextRaster40000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_40000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 40000


class ScrollingTextRaster60000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_60000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 60000


class ScrollingTextRaster75000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_75000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 75000


class ScrollingTextRaster90000Page(ToughFastScrollingPage):
  BASE_NAME = 'text_constant_full_page_raster_90000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_constant_full_page_raster.html'
  SPEED_IN_PIXELS_PER_SECOND = 90000


class ScrollingCanvas5000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_05000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 5000
  TAGS = ToughFastScrollingPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class ScrollingCanvas10000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_10000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 10000


class ScrollingCanvas20000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_20000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 20000


class ScrollingCanvas40000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_40000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 40000


class ScrollingCanvas60000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_60000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 60000


class ScrollingCanvas75000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_75000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 75000


class ScrollingCanvas90000Page(ToughFastScrollingPage):
  BASE_NAME = 'canvas_90000_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/canvas.html'
  SPEED_IN_PIXELS_PER_SECOND = 90000


class NonOpaqueBackgroundMainThreadScrolling00050Page(ToughFastScrollingPage):
  BASE_NAME = 'non_opaque_background_main_thread_scrolling_00050_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_on_non_opaque_background.html'
  EXTRA_BROWSER_ARGS = '--disable-prefer-compositing-to-lcd-text'
  SELECTOR = '#scroll'
  SPEED_IN_PIXELS_PER_SECOND = 50


class NonOpaqueBackgroundCompositorThreadScrolling00050Page(
    ToughFastScrollingPage):
  BASE_NAME = 'non_opaque_background_compositor_thread_scrolling_00050_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text_on_non_opaque_background.html'
  SELECTOR = '#scroll'
  SPEED_IN_PIXELS_PER_SECOND = 50


class ScrollingTextInputOnePerVsyncPlus0Us(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_plus_0us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 0
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


# Test scrolling with different input event timings.
class ScrollingTextInputOnePerVsyncMinus300UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_minus_300us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = -300
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputOnePerVsyncMinus1000UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_minus_1000us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = -1000
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputOnePerVsyncMinus3000UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_minus_3000us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = -3000
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputOnePerVsyncPlus300UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_plus_300us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 300
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputOnePerVsyncPlus1000UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_plus_1000us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 1000
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputOnePerVsyncPlus3000UsPage(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_one_per_vsync_plus_3000us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 3000
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_ONE_PER_VSYNC


class ScrollingTextInputTwoPerVsyncPlus0Us(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_two_per_vsync_plus_0us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 0
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_TWO_PER_VSYNC


class ScrollingTextInputEveryOtherVsyncPlus0Us(ToughFastScrollingPage):
  BASE_NAME = 'text_scroll_input_every_other_vsync_plus_0us'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1000
  VSYNC_OFFSET_US = 0
  INPUT_EVENT_PATTERN = page_action.INPUT_EVENT_PATTERN_EVERY_OTHER_VSYNC
