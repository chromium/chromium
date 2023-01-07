# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class NoOpPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.KEY_NOOP]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(NoOpPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=['--disable-top-sites', '--report-silk-details'])

  def RunNavigateSteps(self, action_runner):
    super(NoOpPage, self).RunNavigateSteps(action_runner)
    # Let load activity settle.
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    # The default page interaction is simply waiting in an idle state.
    with action_runner.CreateInteraction('IdleWaiting'):
      action_runner.Wait(5)


# Why: An infinite rAF loop which does not modify the page should incur
# minimal activity.
class NoOpRafPage(NoOpPage):
  BASE_NAME = 'no_op_raf'
  URL = 'file://../key_noop_cases/no_op_raf.html'


# Why: An infinite setTimeout loop which does not modify the page should
# incur minimal activity.
class NoOpSetTimeoutPage(NoOpPage):
  BASE_NAME = 'no_op_settimeout'
  URL = 'file://../key_noop_cases/no_op_settimeout.html'


class NoOpTouchScrollPage(NoOpPage):
  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # The noop touch motion should last ~5 seconds.
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down', use_touch=True,
                               speed_in_pixels_per_second=300, distance=1500)


# Why: Scrolling an empty, unscrollable page should have no expensive side
# effects, as overscroll is suppressed in such cases.
class NoOpScrollPage(NoOpTouchScrollPage):
  BASE_NAME = 'no_op_scroll'
  URL = 'file://../key_noop_cases/no_op_scroll.html'


# Why: Feeding a stream of touch events to a no-op handler should be cheap.
class NoOpTouchHandlerPage(NoOpTouchScrollPage):
  BASE_NAME = 'no_op_touch_handler'
  URL = 'file://../key_noop_cases/no_op_touch_handler.html'
