# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import py_utils

from telemetry.page import page
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import traffic_setting as traffic_setting_module
from telemetry.page import shared_page_state


_NAVIGATION_TIMEOUT = 180
_WEB_CONTENTS_TIMEOUT = 180

class PageCyclerStory(page.Page):

  def __init__(self, url, page_set,
               shared_page_state_class=shared_page_state.SharedDesktopPageState,
               cache_temperature=cache_temperature_module.ANY, name='',
               traffic_setting=traffic_setting_module.NONE, **kwargs):
    super(PageCyclerStory, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        cache_temperature=cache_temperature, name=name,
        traffic_setting=traffic_setting,
        **kwargs)
    if cache_temperature != cache_temperature_module.ANY:
      self.grouping_keys['cache_temperature'] = cache_temperature
    if traffic_setting != traffic_setting_module.NONE:
      self.grouping_keys['traffic_setting'] = traffic_setting

  def RunNavigateSteps(self, action_runner):
    url = self.file_path_url_with_scheme if self.is_file else self.url
    action_runner.Navigate(url,
                           self.script_to_evaluate_on_commit,
                           timeout_in_seconds=_NAVIGATION_TIMEOUT)

  def RunPageInteractions(self, action_runner):
    py_utils.WaitFor(action_runner.tab.HasReachedQuiescence,
                     _WEB_CONTENTS_TIMEOUT)
    py_utils.WaitFor(action_runner.tab.IsServiceWorkerActivatedOrNotRegistered,
                     _WEB_CONTENTS_TIMEOUT)
    # Wait an extra 5 seconds to give the page a chance to reach First
    # Interactive, so we can compute Time to Interactive and Total Blocking
    # Time.
    action_runner.Wait(5)
