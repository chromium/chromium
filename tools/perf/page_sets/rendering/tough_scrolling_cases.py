# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.internal.actions import page_action
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughFastScrollingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SPEED_IN_PIXELS_PER_SECOND = None
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_DEFAULT
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOUGH_SCROLLING]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughFastScrollingPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(
          direction='down',
          speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND,
          synthetic_gesture_source=self.SYNTHETIC_GESTURE_SOURCE)


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
