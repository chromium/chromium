# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import py_utils
from telemetry import story as story_module
from telemetry.page import page as page_module
from telemetry.page import shared_page_state

class LeakDetectionSharedState(shared_page_state.SharedDesktopPageState):
  def ShouldReuseBrowserForAllStoryRuns(self):
    return True


class LeakDetectionPage(page_module.Page):
  def __init__(self, url, page_set, name=''):
    super(LeakDetectionPage, self).__init__(
      url=url, page_set=page_set, name=name,
      shared_page_state_class=LeakDetectionSharedState)

  def RunNavigateSteps(self, action_runner):
    tabs = action_runner.tab.browser.tabs
    new_tab = tabs.New()
    new_tab.action_runner.Navigate('about:blank')
    new_tab.action_runner.PrepareForLeakDetection()
    new_tab.action_runner.MeasureMemory()
    new_tab.action_runner.Navigate(self.url)
    self._WaitForPageLoadToComplete(new_tab.action_runner)
    new_tab.action_runner.Navigate('about:blank')
    new_tab.action_runner.PrepareForLeakDetection()
    new_tab.action_runner.MeasureMemory()
    new_tab.Close()

  def _WaitForPageLoadToComplete(self, action_runner):
    py_utils.WaitFor(action_runner.tab.HasReachedQuiescence, timeout=30)


# Some websites have a script that loads resources continuously, in which cases
# HasReachedQuiescence would not be reached. This class waits for document ready
# state to be complete to avoid timeout for those pages.
class ResourceLoadingLeakDetectionPage(LeakDetectionPage):
  def _WaitForPageLoadToComplete(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()


class LeakDetectionStorySet(story_module.StorySet):
  def __init__(self):
    super(LeakDetectionStorySet, self).__init__(
      archive_data_file='data/leak_detection.json',
      cloud_storage_bucket=story_module.PARTNER_BUCKET)
    urls_list = [
        # Alexa top websites
        'https://www.google.com',
        'https://www.youtube.com',
        'https://www.facebook.com',
        'https://www.baidu.com',
        'https://www.wikipedia.org',
        'https://world.taobao.com/',
        'https://www.tmall.com/',
        'http://www.amazon.com',
        'http://www.twitter.com',
        'https://www.instagram.com/',
        'http://www.jd.com/',
        'https://vk.com/',
        'https://outlook.live.com',
        'https://www.reddit.com/',
        'https://weibo.com/',
        'https://www.sina.com.cn/',
        'https://www.360.cn/',
        'https://yandex.ru/',
        'https://www.blogger.com/',
        'https://www.netflix.com/',
        'https://www.pornhub.com/',
        'https://www.linkedin.com/',
        'https://www.yahoo.co.jp/',
        'https://www.csdn.net/',
        'https://www.alipay.com/',
        'https://www.twitch.tv/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://www.ebay.com/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://www.microsoft.com/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://www.xvideos.com/',
        'https://mail.ru/',
        'https://www.bing.com/',
        'http://www.wikia.com/',
        'https://www.office.com/',
        'https://www.imdb.com/',
        'https://www.aliexpress.com/',
        'https://www.msn.com/',
        'https://news.google.com/',
        'https://www.theguardian.com/',
        'https://www.indiatimes.com/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'http://www.foxnews.com/',
        'https://weather.com/',
        'https://www.shutterstock.com/',
        'https://docs.google.com/',
        'https://wordpress.com/',
        # TODO(yuzus): This test crashes.
        # 'https://www.apple.com/',
        'https://play.google.com/store',
        'https://www.dropbox.com/',
        'https://soundcloud.com/',
        'https://vimeo.com/',
        'https://www.slideshare.net/',
        'https://www.mediafire.com/',
        'https://www.etsy.com/',
        'https://www.ikea.com/',
        'https://www.bestbuy.com/',
        'https://www.homedepot.com/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://www.target.com/',
        'https://www.booking.com/',
        'https://www.tripadvisor.com/',
        'https://9gag.com/',
        'https://www.expedia.com/',
        'https://www.roblox.com/',
        'https://www.gamespot.com/',
        'https://www.blizzard.com',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://ign.com/',
        'https://www.yelp.com/',
        # Times out waiting for HasReachedQuiescence - crbug.com/927427
        # 'https://gizmodo.com/',
        'https://www.gsmarena.com/',
        'https://www.theverge.com/',
        'https://www.nlm.nih.gov/',
        'https://archive.org/',
        'https://www.udemy.com/',
        'https://answers.yahoo.com/',
        # TODO(crbug.com/41471571): Memory dump fails flakily.
        # 'https://www.goodreads.com/',
        'https://www.cricbuzz.com/',
        'http://www.goal.com/',
        'http://siteadvisor.com/',
        'https://www.patreon.com/',
        'https://www.jw.org/',
        'http://europa.eu/',
        'https://translate.google.com/',
        'https://www.epicgames.com/',
        'http://www.reverso.net/',
        'https://play.na.leagueoflegends.com/',
        'https://www.thesaurus.com/',
        'https://www.weebly.com/',
        'https://www.deviantart.com/',
        'https://www.scribd.com/',
        'https://www.hulu.com/',
        'https://www.xfinity.com/',
        # India Alexa top websites
        'https://porn555.com/',
        'https://www.onlinesbi.com/',
        'https://www.flipkart.com/',
        'https://www.hotstar.com/',
        'https://www.incometaxindiaefiling.gov.in/',
        'https://stackoverflow.com/',
        # TODO(crbug.com/40099304) Memory dump fails flakily.
        # 'https://www.irctc.co.in/nget/',
        'https://www.hdfcbank.com/',
        'https://www.whatsapp.com/',
        'https://uidai.gov.in/',
        'https://billdesk.com/',
        'https://www.icicibank.com/',
        # US Alexa top websites
        'https://imgur.com/',
        'https://www.craigslist.org/',
        'https://www.chase.com/',
        # TODO(crbug.com/41419049): tumblr started timing out due to a catapult
        # roll. See https://crbug.com/892352
        # 'https://www.tumblr.com/',
        'https://www.paypal.com/',
        # TODO(yuzus): espn.com is flaky. https://crbug.com/959796
        #'http://www.espn.com/',
        'https://edition.cnn.com/',
        'https://www.pinterest.com/',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'https://www.nytimes.com/',
        'https://github.com/',
        'https://www.salesforce.com/',
        # Japan Alexa top websites
        'https://www.rakuten.co.jp/',
        'http://www.nicovideo.jp/',
        'https://fc2.com/',
        'https://ameblo.jp/',
        'http://kakaku.com/',
        'https://www.goo.ne.jp/',
        'https://www.pixiv.net/',
        # websites which were found to be leaking in the past
        'https://www.prezi.com',
        # TODO(keishi): Memory dump fails flakily crbug.com/963273
        #'http://www.time.com',
        'http://www.cheapoair.com',
        'http://www.onlinedown.net',
        'http://www.dailypost.ng',
        'http://www.aljazeera.net',
        'http://www.googleapps.com',
        'http://www.airbnb.ch',
        'http://www.livedoor.jp',
        'http://www.blu-ray.com',
        # TODO(crbug.com/40623301): Test times out.
        # 'http://www.block.io',
        'http://www.hockeybuzz.com',
        'http://www.silverpop.com',
        'http://www.ansa.it',
        'http://www.gulfair.com',
        'http://www.nusatrip.com',
        'http://www.samsung-fun.ru',
        'http://www.opentable.com',
        'http://www.magnetmail.net',
        'http://zzz.com.ua',
        'http://a-rakumo.appspot.com',
        'http://www.sakurafile.com',
        'http://www.psiexams.com',
        'http://www.contentful.com',
        'http://www.estibot.com',
        'http://www.mbs.de',
        'http://www.zhengjie.com',
        'http://www.sjp.pl',
        'http://www.mastodon.social',
        'http://www.horairetrain.net',
        'http://www.torrentzeu.to',
        'http://www.inbank.it',
        'http://www.gradpoint.com',
        'http://www.mail.bg',
        'http://www.aaannunci.it',
        'http://www.leandomainsearch.com',
        'http://www.wpjam.com',
        'http://www.nigma.ru',
        'http://www.do-search.com',
        'http://www.omniboxes.com',
        'http://whu.edu.cn',
        'http://support.wordpress.com',
        'http://www.webwebweb.com',
        'http://www.sick.com',
        'http://www.iowacconline.com',
        'http://hdu.edu.cn',
        'http://www.register.com',
        'http://www.careesma.in',
        'http://www.bestdic.ir',
        'http://www.privacyassistant.net',
        'http://www.sklavenzentrale.com',
        'http://www.podbay.fm',
        'http://www.coco.fr',
        'http://www.skipaas.com',
        'http://www.chatword.org',
        'http://www.ezcardinfo.com',
        'http://www.daydao.com',
        'http://www.expediapartnercentral.com',
        'http://www.22find.com',
        'http://www.e-shop.gr',
        'http://www.indeed.com',
        'http://www.highwaybus.com',
        'http://www.pingpang.info',
        'http://www.besgold.com',
        'http://www.arabam.com',
        'http://makfax.com.mk',
        'http://game.co.za',
        'http://www.savaari.com',
        'http://www.railsguides.jp',
    ]
    resource_loading_urls_list = [
      'https://www.hotels.com/',
      'https://www.livejournal.com/',
      # TODO(keishi): Memory dump fails flakily crbug.com/963273
      #'https://www.yahoo.com',
      'http://www.quora.com',
      'https://www.macys.com',
      'http://infomoney.com.br',
      'http://www.listindiario.com',
      'https://www.engadget.com/',
      'https://www.sohu.com/',
      'http://www.qq.com',
      'http://www.benzworld.org',
      'http://www.520mojing.com',
    ]
    for url in urls_list:
      self.AddStory(LeakDetectionPage(url, self, url))
    for url in resource_loading_urls_list:
      self.AddStory(ResourceLoadingLeakDetectionPage(url, self, url))
