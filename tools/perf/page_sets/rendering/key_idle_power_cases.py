# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets import android_screen_restoration_shared_state
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class KeyIdlePowerPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.KEY_IDLE_POWER]
  TURN_SCREEN_OFF =  True
  DURATION_SECONDS = 20

  def __init__(self,
               page_set,
               shared_page_state_class=(android_screen_restoration_shared_state.
                                        AndroidScreenRestorationSharedState),
               name_suffix='',
               extra_browser_args=None,
               perform_final_navigation=False):
    super(KeyIdlePowerPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=['--report-silk-details', '--disable-top-sites'],
        perform_final_navigation=perform_final_navigation)

  def RunNavigateSteps(self, action_runner):
    super(KeyIdlePowerPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    if self.TURN_SCREEN_OFF:
      action_runner.tab.browser.platform.android_action_runner.TurnScreenOff()
      # We're not interested in tracking activity that occurs immediately after
      # the screen is turned off. Several seconds should be enough time for the
      # browser to "settle down" into an idle state.
      action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    # The page interaction is simply waiting in an idle state.
    with action_runner.CreateInteraction('IdleWaiting'):
      action_runner.Wait(self.DURATION_SECONDS)


# Why: Ensure minimal activity for static, empty pages in the foreground.
class BlankIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_blank'
  URL = 'file://../key_idle_power_cases/blank.html'
  TURN_SCREEN_OFF = False


# Why: Ensure animated GIFs aren't processed when Chrome is backgrounded.
class AnimatedGifIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_animated_gif'
  URL = 'file://../key_idle_power_cases/animated-gif.html'


# Why: Ensure CSS animations aren't processed when Chrome is backgrounded.
class CssAnimationIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_css_animation'
  URL = 'file://../key_idle_power_cases/css-animation.html'


# Why: Ensure rAF is suppressed when Chrome is backgrounded.
class RequestAnimationIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_request_animation_frame'
  URL = 'file://../key_idle_power_cases/request-animation-frame.html'


# Why: Ensure setTimeout is throttled when Chrome is backgrounded.
class SetTimeoutIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_set_timetout'
  URL = 'file://../key_idle_power_cases/set-timeout.html'


# Why: Ensure that activity strictly diminishes the longer the idle time.
class SetTimeoutLongIdlePowerPage(KeyIdlePowerPage):
  BASE_NAME = 'idle_power_set_timeout_long'
  URL = 'file://../key_idle_power_cases/set-timeout.html'
  # 90 seconds ensures the capture of activity after the 60-second
  # PowerMonitor suspend signal.
  DURATION_SECONDS = 90
