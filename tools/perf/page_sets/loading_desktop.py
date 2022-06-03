# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from page_sets import page_cycler_story
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import shared_page_state
from telemetry import story

Tag = collections.namedtuple('Tag', ['name', 'description'])

# pylint: disable=line-too-long
# Used https://cs.chromium.org/chromium/src/tools/perf/experimental/story_clustering/README.md
# to find representative stories.

ABRIDGED = Tag('abridged', 'Story should be included in abridged runs')
ABRIDGED_STORY_NAMES = [
    "AirBnB_warm",
    "ru.wikipedia_warm",
    "Baidu_warm",
    "AllRecipes_cold",
    "TheOnion_cold",
    "Naver_cold",
    "Aljayyash_cold",
    "Taobao_warm",
    "Orange_cold",
    "Orange_warm",
]


class LoadingDesktopStorySet(story.StorySet):

  """ A collection of tests to measure loading performance of desktop sites.

  Desktop centric version of loading_mobile.py
  """

  def __init__(self, cache_temperatures=None):
    super(LoadingDesktopStorySet, self).__init__(
        archive_data_file='data/loading_desktop.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    if cache_temperatures is None:
      cache_temperatures = [
          cache_temperature_module.COLD, cache_temperature_module.WARM
      ]
    # Passed as (story, name) tuple.
    self.AddStories(
        ['intl_ar_fa_he', 'international'],
        [('http://farsnews.com/', 'FarsNews'),
         ('http://www.aljayyash.net/', 'Aljayyash'),
         ('http://haraj.com.sa', 'Haraj')], cache_temperatures)
    self.AddStories(
        ['intl_es_fr_pt_BR', 'international'],
        [('http://elmundo.es/', 'Elmundo'),
         ('http://www.free.fr/adsl/index.html', 'Free.fr'),
         ('http://www.orange.fr/', 'Orange'),
         ('http://www.uol.com.br/', 'uol.com.br'),
         ('http://www.mercadolivre.com.br/', 'Mercadolivre')],
        cache_temperatures)
    self.AddStories(
        ['intl_hi_ru', 'international'],
        [('https://yandex.ru/search/?text=google', 'Yandex'),
         ('https://ru.wikipedia.org/wiki/%D0%9C%D0%BE%D1%81%D0%BA%D0%B2%D0%B0',
             'ru.wikipedia'),
         ('http://www.indiatimes.com/', 'IndiaTimes'),
         ('http://www.bhaskar.com/', 'Bhaskar'),
         ('http://www.flipkart.com', 'FlipKart')], cache_temperatures)
    self.AddStories(
        ['intl_ja_zh', 'international'],
        [('http://www.amazon.co.jp', 'amazon.co.jp'),
         ('http://b.hatena.ne.jp/hotentry', 'HatenaBookmark'),
         ('http://www.yahoo.co.jp/', 'yahoo.co.jp'),
         ('http://fc2information.blog.fc2.com/', 'FC2Blog'),
         ('http://kakaku.com/', 'Kakaku'),
         # pylint: disable=line-too-long
         ('https://ja.wikipedia.org/wiki/%E3%82%B4%E3%83%AB%E3%82%B413%E3%81%AE%E3%82%A8%E3%83%94%E3%82%BD%E3%83%BC%E3%83%89%E4%B8%80%E8%A6%A7',
             'ja.wikipedia'),
         ('http://www.baidu.com/s?wd=%D0%C2%20%CE%C5', 'Baidu'),
         ('http://www.qq.com/', 'QQ'),
         ('http://www.taobao.com/index_global.php', 'Taobao'),
         ('http://www.sina.com.cn/', 'Sina'),
         ('http://ruten.com.tw/', 'Ruten')], cache_temperatures)
    self.AddStories(
        ['intl_ko_th_vi', 'international'],
        [('http://us.24h.com.vn/', '24h'),
         ('http://vnexpress.net/', 'Vnexpress'),
         ('http://vietnamnet.vn/', 'Vietnamnet'),
         ('http://kenh14.vn/home.chn', 'Kenh14'),
         ('http://www.naver.com/', 'Naver'),
         ('http://www.daum.net/', 'Daum'),
         ('http://www.donga.com/', 'Donga'),
         ('http://www.chosun.com/', 'Chosun'),
         ('http://www.danawa.com/', 'Danawa'),
         ('http://pantip.com/', 'Pantip')], cache_temperatures)
    self.AddStories(
        ['typical'],
        [('http://www.rei.com/', 'REI'),
         ('http://www.fifa.com/', 'FIFA'),
         ('http://www.economist.com/', 'Economist'),
         ('http://www.theonion.com', 'TheOnion'),
         ('http://arstechnica.com/', 'ArsTechnica'),
         ('http://allrecipes.com/recipe/239896/crunchy-french-onion-chicken',
             'AllRecipes'),
         ('http://www.html5rocks.com/en/', 'HTML5Rocks'),
         ('http://www.imdb.com/title/tt0910970/', 'IMDB'),
         ('http://www.flickr.com/search/?q=monkeys&f=hp', 'Flickr'),
         ('http://money.cnn.com/', 'money.cnn'),
         ('http://premierleague.com', 'PremierLeague'),
         ('http://walgreens.com', 'Walgreens'),
         ('http://colorado.edu', 'Colorado.edu'),
         ('http://www.ticketmaster.com/', 'TicketMaster'),
         ('http://www.theverge.com/', 'TheVerge'),
         ('http://www.airbnb.com/', 'AirBnB'),
         ('http://www.ign.com/', 'IGN')], cache_temperatures)

  def GetAbridgedStorySetTagFilter(self):
    return ABRIDGED.name

  def AddStories(self, tags, urls, cache_temperatures):
    for url, name in urls:
      for temp in cache_temperatures:
        if temp == cache_temperature_module.COLD:
          page_name = name + '_cold'
          tags.append('cache_temperature_cold')
        elif temp == cache_temperature_module.WARM:
          page_name = name + '_warm'
          tags.append('cache_temperature_warm')
        else:
          raise NotImplementedError

        page_tags = tags[:]

        if page_name in ABRIDGED_STORY_NAMES:
          page_tags.append(ABRIDGED.name)

        self.AddStory(
            page_cycler_story.PageCyclerStory(
                url,
                self,
                shared_page_state_class=shared_page_state.
                SharedDesktopPageState,
                cache_temperature=temp,
                tags=page_tags,
                name=page_name,
                perform_final_navigation=True))
