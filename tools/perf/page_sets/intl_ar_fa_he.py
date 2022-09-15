# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import page_cycler_story
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import shared_page_state
from telemetry import story


class IntlArFaHePage(page_cycler_story.PageCyclerStory):

  def __init__(self, url, page_set, cache_temperature=None):
    if cache_temperature == cache_temperature_module.COLD:
      temp_suffix = '_cold'
    elif cache_temperature == cache_temperature_module.WARM:
      temp_suffix = '_warm'
    else:
      raise NotImplementedError

    super(IntlArFaHePage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state.SharedDesktopPageState,
        cache_temperature=cache_temperature,
        name=url + temp_suffix)


class IntlArFaHePageSet(story.StorySet):

  """ Popular pages in right-to-left languages Arabic, Farsi and Hebrew. """

  def __init__(self, cache_temperatures=(cache_temperature_module.COLD,
                                         cache_temperature_module.WARM)):
    super(IntlArFaHePageSet, self).__init__(
      archive_data_file='data/intl_ar_fa_he.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    if cache_temperatures is None:
      cache_temperatures = [cache_temperature_module.ANY]

    urls_list = [
      'http://msn.co.il/',
      'http://ynet.co.il/',
      'http://www.islamweb.net/',
      'http://farsnews.com/',
      'http://www.masrawy.com/',
      'http://www.startimes.com/f.aspx',
      'http://www.aljayyash.net/',
      'http://www.google.com.sa/'
    ]

    for url in urls_list:
      for temp in cache_temperatures:
        self.AddStory(IntlArFaHePage(url, self, cache_temperature=temp))
