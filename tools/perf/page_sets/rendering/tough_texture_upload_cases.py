# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughTextureUploadPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_TEXTURE_UPLOAD]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix="",
               extra_browser_args=None):
    super(ToughTextureUploadPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Animation'):
      action_runner.Wait(10)


class BackgroundColorAnimationPage(ToughTextureUploadPage):
  BASE_NAME = 'background_color_animation'
  URL = 'file://../tough_texture_upload_cases/background_color_animation.html'


class BackgroundColorAnimationWithGradientPage(ToughTextureUploadPage):
  BASE_NAME = 'background_color_animation_with_gradient'
  # pylint: disable=line-too-long
  URL = 'file://../tough_texture_upload_cases/background_color_animation_with_gradient.html'


class SmallTextureUploadsPage(ToughTextureUploadPage):
  BASE_NAME = 'small_texture_uploads'
  URL = 'file://../tough_texture_upload_cases/small_texture_uploads.html'


class MediumTextureUploadsPage(ToughTextureUploadPage):
  BASE_NAME = 'medium_texture_uploads'
  URL = 'file://../tough_texture_upload_cases/medium_texture_uploads.html'


class LargeTextureUploadsPage(ToughTextureUploadPage):
  BASE_NAME = 'large_texture_uploads'
  URL = 'file://../tough_texture_upload_cases/large_texture_uploads.html'


class ExtraLargeTextureUploadsPage(ToughTextureUploadPage):
  BASE_NAME = 'extra_large_texture_uploads'
  URL = 'file://../tough_texture_upload_cases/extra_large_texture_uploads.html'
  TAGS = ToughTextureUploadPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE
  ]
