# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.internal.actions import page_action
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags

TOUGH_SCROLLBAR_UMA = [
    'Graphics.Smoothness.Checkerboarding4.ScrollbarScroll',
]


class ToughFastScrollbarScrollingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SPEED_IN_PIXELS_PER_SECOND = None
  SYNTHETIC_GESTURE_SOURCE = page_action.GESTURE_SOURCE_MOUSE
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOUGH_SCROLLBAR_SCROLLING]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughFastScrollbarScrollingPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    scrollbar_x, scrollbar_top, scrollbar_bottom, top_button, bottom_button = \
      self._ScrollBarRatios(action_runner)
    start = time.time()
    with action_runner.CreateGestureInteraction('DragAction'):
      direction = 'down'
      while time.time() - start < 15:
        if direction == 'down':
          action_runner.DragPage(
              left_start_ratio=scrollbar_x,
              top_start_ratio=scrollbar_top,
              left_end_ratio=scrollbar_x,
              top_end_ratio=bottom_button,
              speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND)
        else:
          action_runner.DragPage(
              left_start_ratio=scrollbar_x,
              top_start_ratio=scrollbar_bottom,
              left_end_ratio=scrollbar_x,
              top_end_ratio=top_button,
              speed_in_pixels_per_second=self.SPEED_IN_PIXELS_PER_SECOND)
        direction = 'up' if direction == 'down' else 'down'

  def _ScrollBarRatios(self, action_runner):
    # Calculate scrollbar thickness by adding an element to the page.
    # Record the client width of that element, and then add a long child
    # element so that the first element includes a scrollbar. Record the
    # client width again and then calculate the difference to get the
    # scrollbar thickness.
    scrollbar_thickness = float(
        action_runner.EvaluateJavaScript('''
        (function() {
          window.__scrollableElementForTelemetry = document.scrollingElement;
          var container = document.createElement("div");
          container.style.width = "100px";
          container.style.height = "100px";
          container.style.position = "absolute";
          container.style.visibility = "hidden";
          container.style.overflow = "auto";

          document.body.appendChild(container);

          var widthBefore = container.clientWidth;
          var longContent = document.createElement("div");
          longContent.style.height = "1000px";
          container.appendChild(longContent);

          var widthAfter = container.clientWidth;

          container.remove();

          window.__scrollbarThickness = widthBefore - widthAfter;
          return window.__scrollbarThickness;
        })();'''))
    window_width = float(action_runner.EvaluateJavaScript('window.innerWidth'))
    window_height = float(
        action_runner.EvaluateJavaScript('window.innerHeight'))
    # Works only when there is no horizontally overflowing content.
    left_margin = float(
        action_runner.EvaluateJavaScript(
            'document.body.getBoundingClientRect().x'))
    body_width = float(
        action_runner.EvaluateJavaScript(
            'document.body.getBoundingClientRect().width'))
    top_margin = float(
        action_runner.EvaluateJavaScript(
            'document.body.getBoundingClientRect().y'))
    window_height = window_height - top_margin

    # For regular scrollbars the minimal thumb length is one and two times the
    # track's width in W10 and W11 respectively. For Fluent scrollbars the
    # minimal thumb length is 17px. The constant was chosen such that:
    # (Button Length) <= (Track width * |track_to_arrow_button_ratio|) <=
    # (Button Length + Minimum thumb length) for all scrollbars.
    track_to_arrow_button_ratio = 1.8
    top = (scrollbar_thickness * track_to_arrow_button_ratio -
           top_margin) / (window_height)
    bottom = 1 - (scrollbar_thickness *
                  track_to_arrow_button_ratio) / window_height
    scrollbar_mid_x = (window_width - left_margin -
                       (scrollbar_thickness / 2)) / body_width
    # Ensure the thumb goes through the full range of motion. These
    # variables make the the mouse drag until the middle of the scrollbar
    # buttons.
    top_button = ((scrollbar_thickness / 2) - top_margin) / window_height
    bottom_button = 1 - (scrollbar_thickness / 2) / window_height
    return scrollbar_mid_x, top, bottom, top_button, bottom_button

  def WillStartTracing(self, chrome_trace_config):
    chrome_trace_config.EnableUMAHistograms(*TOUGH_SCROLLBAR_UMA)


class ScrollbarScrollingText100Page(ToughFastScrollbarScrollingPage):
  BASE_NAME = 'text_scrollbar_100_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 100


class ScrollbarScrollingText200Page(ToughFastScrollbarScrollingPage):
  BASE_NAME = 'text_scrollbar_200_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 400


class ScrollbarScrollingText300Page(ToughFastScrollbarScrollingPage):
  BASE_NAME = 'text_scrollbar_700_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 700


class ScrollbarScrollingText1200Page(ToughFastScrollbarScrollingPage):
  BASE_NAME = 'text_scrollbar_1200_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 1200


class ScrollbarScrollingText2300Page(ToughFastScrollbarScrollingPage):
  BASE_NAME = 'text_scrollbar_2300_pixels_per_second'
  URL = 'file://../tough_scrolling_cases/text.html'
  SPEED_IN_PIXELS_PER_SECOND = 2300
