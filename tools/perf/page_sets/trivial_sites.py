# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class _BasePage(page_module.Page):
  def __init__(
      self, page_set, shared_page_state_class, url, name, wait_in_seconds,
      measure_memory):
    super(_BasePage, self).__init__(
        url=url, page_set=page_set, name=name,
        shared_page_state_class=shared_page_state_class)
    self._wait_in_seconds = wait_in_seconds
    self.measure_memory = measure_memory

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(self._wait_in_seconds)
    if self.measure_memory:
      action_runner.MeasureMemory(deterministic_mode=True)


class TrivialScrollingPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialScrollingPage, self).__init__(
        url='file://trivial_sites/trivial_scrolling_page.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialBlinkingCursorPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
           measure_memory):
    super(TrivialBlinkingCursorPage, self).__init__(
        url='file://trivial_sites/trivial_blinking_cursor.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialCanvasPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialCanvasPage, self).__init__(
        url='file://trivial_sites/trivial_canvas.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialWebGLPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialWebGLPage, self).__init__(
        url='file://trivial_sites/trivial_webgl.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialBlurAnimationPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialBlurAnimationPage, self).__init__(
        url='file://trivial_sites/trivial_blur_animation.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialFullscreenVideoPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialFullscreenVideoPage, self).__init__(
        url='file://trivial_sites/trivial_fullscreen_video.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)

  def RunPageInteractions(self, action_runner):
    action_runner.PressKey("Return")
    super(TrivialFullscreenVideoPage, self).RunPageInteractions(action_runner)


class TrivialGifPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialGifPage, self).__init__(
        url='file://trivial_sites/trivial_gif.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialAnimationPage(_BasePage):

  def __init__(self, page_set, shared_page_state_class, wait_in_seconds,
               measure_memory):
    super(TrivialAnimationPage, self).__init__(
        url='file://trivial_sites/trivial_animation.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class,
        wait_in_seconds=wait_in_seconds,
        measure_memory=measure_memory)


class TrivialSitesStorySet(story.StorySet):
  def __init__(self, shared_state = shared_page_state.SharedPageState,
               wait_in_seconds=0, measure_memory=False):
    # Wait is time to wait_in_seconds on page in seconds.
    super(TrivialSitesStorySet, self).__init__(
        cloud_storage_bucket=story.PUBLIC_BUCKET)
    self.AddStory(TrivialScrollingPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialBlinkingCursorPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialCanvasPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialWebGLPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialBlurAnimationPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialFullscreenVideoPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialGifPage(
        self, shared_state, wait_in_seconds, measure_memory))
    self.AddStory(TrivialAnimationPage(
        self, shared_state, wait_in_seconds, measure_memory))
