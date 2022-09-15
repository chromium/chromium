# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import traffic_setting as traffic_setting_module
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class CTPage(page_module.Page):

  def __init__(self, url, page_set, shared_page_state_class, archive_data_file,
               traffic_setting, run_page_interaction_callback,
               run_navigate_steps_callback, cache_temperature):
    super(CTPage, self).__init__(
        url=url,
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        traffic_setting=traffic_setting,
        cache_temperature=cache_temperature,
        grouping_keys={'temperature': cache_temperature},
        name=url)
    self.archive_data_file = archive_data_file
    self._run_navigate_steps_callback = run_navigate_steps_callback
    self._run_page_interaction_callback = run_page_interaction_callback

  def RunNavigateSteps(self, action_runner):
    if self._run_navigate_steps_callback:
      self._run_navigate_steps_callback(self, action_runner)
    else:
      action_runner.Navigate(self.url)

  def RunPageInteractions(self, action_runner):
    if self._run_page_interaction_callback:
      self._run_page_interaction_callback(action_runner)


class CTPageSet(story.StorySet):
  """Page set used by CT Benchmarks."""

  def __init__(self, urls_list, user_agent, archive_data_file,
               traffic_setting=traffic_setting_module.NONE,
               run_page_interaction_callback=None,
               run_navigate_steps_callback=None,
               cache_temperature=cache_temperature_module.ANY):
    if user_agent == 'mobile':
      shared_page_state_class = shared_page_state.SharedMobilePageState
    elif user_agent == 'desktop':
      shared_page_state_class = shared_page_state.SharedDesktopPageState
    else:
      raise ValueError('user_agent %s is unrecognized' % user_agent)

    super(CTPageSet, self).__init__(archive_data_file=archive_data_file)

    for url in urls_list.split(','):
      self.AddStory(
          CTPage(url, self, shared_page_state_class=shared_page_state_class,
                archive_data_file=archive_data_file,
                traffic_setting=traffic_setting,
                run_page_interaction_callback=run_page_interaction_callback,
                run_navigate_steps_callback=run_navigate_steps_callback,
                cache_temperature=cache_temperature))
