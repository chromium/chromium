# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import page_cycler_story
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import shared_page_state
from telemetry import story


class IntlHiRuPage(page_cycler_story.PageCyclerStory):

  def __init__(self, url, page_set, cache_temperature=None):
    if cache_temperature == cache_temperature_module.COLD:
      temp_suffix = '_cold'
    elif cache_temperature == cache_temperature_module.WARM:
      temp_suffix = '_warm'
    else:
      raise NotImplementedError

    super(IntlHiRuPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state.SharedDesktopPageState,
        cache_temperature=cache_temperature,
        name=url+temp_suffix)


class IntlHiRuPageSet(story.StorySet):

  """ Popular pages in Hindi and Russian. """

  def __init__(self, cache_temperatures=(cache_temperature_module.COLD,
                                         cache_temperature_module.WARM)):
    super(IntlHiRuPageSet, self).__init__(
      archive_data_file='data/intl_hi_ru.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    if cache_temperatures is None:
      cache_temperatures = [cache_temperature_module.ANY]

    urls_list = [
      # Why: #12 site in Russia
      'http://www.rambler.ru/',
      'http://apeha.ru/',
      # pylint: disable=line-too-long
      'http://yandex.ru/yandsearch?lr=102567&text=%D0%9F%D0%BE%D0%B3%D0%BE%D0%B4%D0%B0',
      'http://photofile.ru/',
      'http://ru.wikipedia.org/',
      'http://narod.yandex.ru/',
      # Why: #15 in Russia
      'http://rutracker.org/forum/index.php',
      'http://hindi.webdunia.com/',
      # Why: #49 site in India
      'http://hindi.oneindia.in/',
      # Why: #9 site in India
      'http://www.indiatimes.com/',
      # Why: #2 site in India
      'http://news.google.co.in/nwshp?tab=in&hl=hi'
    ]

    for url in urls_list:
      for temp in cache_temperatures:
        self.AddStory(IntlHiRuPage(url, self, cache_temperature=temp))
