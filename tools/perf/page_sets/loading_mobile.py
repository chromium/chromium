# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from page_sets import page_cycler_story
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import shared_page_state
from telemetry.page import traffic_setting as traffic_setting_module
from telemetry import story

Tag = collections.namedtuple('Tag', ['name', 'description'])

# pylint: disable=line-too-long
# Used https://cs.chromium.org/chromium/src/tools/perf/experimental/story_clustering/README.md
# to find representative stories.

ABRIDGED = Tag('abridged', 'Story should be included in abridged runs')
ABRIDGED_STORY_NAMES = [
    "Facebook",
    "GoogleIndia",
    "LocalMoxie",
    "Dramaq",
    "Hongkiat",
    "FranceTVInfo",
]


class LoadingMobileStorySet(story.StorySet):

  """ A collection of tests to measure loading performance of mobile sites.

  Design doc: https://docs.google.com/document/d/1QKlZIoURAxZk-brrXsKYZl9O8ieqXht3ogeF9yLNFCI/edit
  """

  def __init__(self,
               cache_temperatures=None,
               cache_temperatures_for_pwa=None,
               traffic_settings=None,
               include_tags=None):
    super(LoadingMobileStorySet, self).__init__(
        archive_data_file='data/loading_mobile.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    if include_tags is None:
      include_tags = ['*']

    if cache_temperatures is None:
      cache_temperatures = [cache_temperature_module.ANY]

    if cache_temperatures_for_pwa is None:
      cache_temperatures_for_pwa = [cache_temperature_module.ANY]

    if traffic_settings is None:
      traffic_settings = [traffic_setting_module.NONE]

    if '*' in include_tags or 'global' in include_tags:
      self.AddStories(['global'], [
          ('https://www.google.com/search?q=flower#q=flower+delivery',
           'Google'),
          ('https://www.google.com/search?q=flower#q=flower+delivery',
           'GoogleRedirectToGoogleJapan'),
          ('https://www.youtube.com/watch?v=MU3YuvNRhVY', 'Youtube'),
          # pylint: disable=line-too-long
          ('https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2&rct=j#q=%E0%A4%AB%E0%A5%82%E0%A4%B2+%E0%A4%B5%E0%A4%BF%E0%A4%A4%E0%A4%B0%E0%A4%A3',
           'GoogleIndia'),
          ('https://www.google.com.br/search?q=flor#q=Entrega+de+flores&start=10',
           'GoogleBrazil'),
          ('https://www.google.co.id/#q=pengiriman+bunga', 'GoogleIndonesia'),
          ('https://m.facebook.com/?soft=messages', 'Facebook'),
          # pylint: disable=line-too-long
          ('http://g1.globo.com/politica/noticia/2016/02/maioria-do-stf-autoriza-fisco-obter-dados-bancarios-sem-decisao-judicial.html',
           'G1'),
          # pylint: disable=line-too-long
          ('https://m.baidu.com/s?word=%E9%B2%9C%E8%8A%B1%E9%80%9F%E9%80%92&oq=%E9%B2%9C%E8%8A%B1',
           'Baidu'),
          # pylint: disable=line-too-long
          ('http://news.yahoo.com/were-top-10-most-visited-us-national-parks-105323727.html',
           'YahooNews'),
          ('https://en.m.wikipedia.org/wiki/Solo_Foods', 'Wikipedia'),
          # pylint: disable=line-too-long
          ('http://noticias.bol.uol.com.br/ultimas-noticias/brasil/2016/08/03/tufao-nida-nao-deixa-vitimas-mas-prejuizos-de-us-43-milhoes.htm',
           'BOLNoticias'),
          ('http://www.amazon.com/gp/aw/s/ref=is_s/189-8585431-1246432?k=shoes',
           'Amazon'),
          # pylint: disable=line-too-long
          ('http://m.tribunnews.com/superskor/2016/08/03/ribuan-polisi-dikerahkan-mengawal-bonek',
           'TribunNews'),
          ('http://xw.qq.com/news/20160803025029/NEW2016080302502901', 'QQNews'),
          # pylint: disable=line-too-long
          ('http://m.kaskus.co.id/thread/57a03a3214088d91068b4567/inilah-akibat-bersikap-overprotektif-terhadap-anak/?ref=homelanding&med=hot_thread',
           'Kaskus'),
          ('http://www.dailymotion.com/video/x3d1kj5_fallout-4-review_videogames',
           'Dailymotion'),
          ('https://mobile.twitter.com/scottjehl/status/760618697727803394',
           'Twitter'),
          ('http://m.kapanlagi.com/lirik/artis/anji/kata_siapa/',
           'KapanLagi'),
          # pylint: disable=line-too-long
          ('http://olx.co.id/iklan/iphone-6s-64-rose-gold-warna-favorite-IDiSdm5.html#5310a118c3;promoted',
           'OLX'),
          # pylint: disable=line-too-long
          ('http://enquiry.indianrail.gov.in/mntes/MntesServlet?action=MainMenu&subAction=excep&excpType=EC',
           'EnquiryIndianRail'),
          # TODO(rnephew): Rerecord this. crbug.com/728882
          # pylint: disable=line-too-long
          # ('https://googleblog.blogspot.jp/2016/02/building-safer-web-for-everyone.html',
          #  'Blogspot'),
          # pylint: disable=line-too-long
          # ('http://m.detik.com/finance/read/2016/02/19/151843/3146351/1034/ekspor-tambang-mentah-mau-dibuka-lagi-kalau-sudah-bangun-smelter-bagaimana',
          #  'Detik'),
      ], cache_temperatures, traffic_settings)

    if '*' in include_tags or 'pwa' in include_tags:
      self.AddStories(['pwa'], [
          # pylint: disable=line-too-long
          ('https://www.flipkart.com/big-wing-casuals/p/itmemeageyfn6m9z?lid=LSTSHOEMEAGURG2PHPW18FTBN&pid=SHOEMEAGURG2PHPW',
           'FlipKart'),
          ('https://smp.suumo.jp/mansion/tokyo/sc_104/cond/?moreCond=1',
           'Suumo'),
          ('https://voice-memos.appspot.com', 'VoiceMemos'),
          ('https://dev.opera.com/', 'DevOpera'),
          ('https://flipboard.com/topic/yoga', 'FlipBoard'),
          # TODO(rnephew): Record these. crbug.com/728882
          # ('https://wiki-offline.jakearchibald.com/',
          #  'WikiOffline'),
          # ('https://busrouter.sg', 'BusRouter'),
          # ('https://airhorner.com', 'AirHorner'),
      ], cache_temperatures_for_pwa, traffic_settings)

    if '*' in include_tags or 'tough_ttfmp' in include_tags:
      self.AddStories(['tough_ttfmp'], [
          ('http://www.localmoxie.com', 'LocalMoxie'),
          ('http://www.dawn.com', 'Dawn'),
          ('http://www.thairath.co.th', 'Thairath'),
      ], cache_temperatures, traffic_settings)

    if '*' in include_tags or 'easy_ttfmp' in include_tags:
      self.AddStories(['easy_ttfmp'], [
          ('http://www.slideshare.net', 'SlideShare'),
          ('http://www.bradesco.com.br', 'Bradesco'),
          ('http://www.gsshop.com', 'GSShop'),
      ], cache_temperatures, traffic_settings)

    if '*' in include_tags or 'tough_tti' in include_tags:
      self.AddStories(['tough_tti'], [
          ('http://www.thestar.com.my', 'TheStar'),
          ('http://www.58pic.com', '58Pic'),
          ('http://www.hongkiat.com', 'Hongkiat'),
      ], cache_temperatures, traffic_settings)

    if '*' in include_tags or 'easy_tti' in include_tags:
      self.AddStories(['easy_tti'], [
          ('http://www.dramaq.com.tw', 'Dramaq'),
          ('http://www.locanto.in', 'Locanto'),
          ('http://www.francetvinfo.fr', 'FranceTVInfo'),
      ], cache_temperatures, traffic_settings)

    # The many_agents stories are not added unless explicitly specified to keep
    # loading.{mobile,desktop} benchmarks short.
    if 'many_agents' in include_tags:
      self.AddStories(['many_agents'], [
          ('https://www.amazon.co.jp', 'AmazonJP'),
          ('https://www.netflix.com', 'Netflix'),
          ('https://sina.com.cn', 'Sina'),
          ('https://www.ebay.com', 'Ebay'),
          ('https://www.msn.com', 'MSN'),
          ('https://www.imdb.com', 'IMDB'),
          ('https://www.cnn.com', 'CNN'),
          ('https://www.bbc.com', 'BBC'),
          ('https://www.espn.com', 'ESPN'),
          ('https://www.nytimes.com', 'NyTimes'),
          ('https://www.nicovideo.jp', 'Nicovideo'),
          ('https://www.cricbuzz.com', 'Cricbuzz'),
          ('https://www.theguardian.com', 'TheGuardian'),
          ('https://www.cnet.com', 'CNet'),
          ('https://www.indiatimes.com', 'IndiaTimes'),
          ('https://www.sindonews.com', 'SindoNews'),
      ], cache_temperatures, traffic_settings)

  def GetAbridgedStorySetTagFilter(self):
    return ABRIDGED.name

  def AddStories(self, tags, urls, cache_temperatures, traffic_settings):
    for url, name in urls:
      for temp in cache_temperatures:
        for traffic in traffic_settings:
          page_name = name
          if temp == cache_temperature_module.COLD:
            page_name += '_cold'
            tags.append('cache_temperature_cold')
          elif temp == cache_temperature_module.WARM:
            page_name += '_warm'
            tags.append('cache_temperature_warm')
          elif temp == cache_temperature_module.HOT:
            page_name += '_hot'
            tags.append('cache_temperature_hot')
          elif temp == cache_temperature_module.ANY:
            pass
          else:
            raise NotImplementedError

          if traffic == traffic_setting_module.REGULAR_3G:
            page_name += '_3g'

          page_tags = tags[:]

          if page_name in ABRIDGED_STORY_NAMES:
            page_tags.append(ABRIDGED.name)

          self.AddStory(
              page_cycler_story.PageCyclerStory(
                  url,
                  self,
                  name=page_name,
                  shared_page_state_class=shared_page_state.
                  SharedMobilePageState,
                  cache_temperature=temp,
                  traffic_setting=traffic,
                  tags=page_tags,
                  perform_final_navigation=True))
