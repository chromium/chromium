# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags

# pylint: disable=line-too-long
_MAPS_PERF_TEST_DIR = os.path.join(os.path.dirname(__file__), '../maps_perf_test')

class MapsPage(rendering_story.RenderingStory):
  """Google Maps benchmarks and pixel tests.

  The Maps team gave us a build of their test. The static files are stored in
  //src/tools/perf/page_sets/maps_perf_test/.

  Note: the file maps_perf_test/load_dataset is a large binary file (~3Mb),
  hence we upload it to cloud storage & only check in the SHA1 hash.

  The command to upload it to cloud_storage is:
  <path to depot_tools>/upload_to_google_storage.py \
      maps_perf_test/load_dataset --bucket=chromium-telemetry
"""
  BASE_NAME = 'maps_perf_test'
  URL = 'file://performance.html'
  TAGS = [story_tags.REQUIRED_WEBGL, story_tags.MAPS]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(MapsPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        base_dir=_MAPS_PERF_TEST_DIR)

  @property
  def skipped_gpus(self):
    # Skip this intensive test on low-end devices. crbug.com/464731
    return ['arm']

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition('window.startTest !== undefined')
    action_runner.EvaluateJavaScript('startTest()')
    with action_runner.CreateInteraction('MapAnimation'):
      action_runner.WaitForJavaScriptCondition('window.testDone', timeout=120)
