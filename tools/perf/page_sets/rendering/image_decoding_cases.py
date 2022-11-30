# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ImageDecodingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.IMAGE_DECODING]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ImageDecodingPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('DecodeImage'):
      action_runner.Wait(5)


class WebPDecodingPage(ImageDecodingPage):
  BASE_NAME = 'webp_decoding'
  URL = 'file://../image_decoding_cases/webp_decoding.html'
  TAGS = ImageDecodingPage.TAGS + [story_tags.GPU_RASTERIZATION]

class JpegDecodingPage(ImageDecodingPage):
  BASE_NAME = 'jpeg_decoding'
  URL = 'file://../image_decoding_cases/jpeg_decoding.html'
  TAGS = ImageDecodingPage.TAGS + [story_tags.GPU_RASTERIZATION]
