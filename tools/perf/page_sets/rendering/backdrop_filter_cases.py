# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class BackdropFilterPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.BACKDROP_FILTER]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(BackdropFilterPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(BackdropFilterPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition('window.readyToStart')

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('LetAnimationRun'):
      action_runner.ExecuteJavaScript('animateOneFrame()')
      action_runner.WaitForJavaScriptCondition('window.animationDone')


# Why: should hopefully get 60 fps when moving foregrounds blur a rotating
# background.
class BlurRotatingBackgroundPage(BackdropFilterPage):
  BASE_NAME = 'blur_rotating_background'
  URL = 'file://../backdrop_filter_cases/blur_rotating_background.html'
