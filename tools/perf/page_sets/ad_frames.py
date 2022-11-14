# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

URL_LIST = [
    'https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B22225778.261324265;sz=300x250',
    'https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B22225778.238871593;sz=300x600',
    'https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B22225778.256945163;sz=320x50',
    'https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B8455269.127425649;sz=970x250',
    'https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B8455269.129556071;sz=970x250',
]


class PageWithFrame(page_module.Page):
  def __init__(self, frame_type, frame_url, page_set, name=''):
    self.frame_url = frame_url
    self.frame_type = frame_type
    if name == '':
      name = frame_type + ": " + frame_url
    super(PageWithFrame, self).__init__(
        'file://ad_frames/loader.html',
        page_set=page_set,
        name=name,
        shared_page_state_class=shared_page_state.SharedDesktopPageState,
        extra_browser_args=[
            '''--enable-features=PrivacySandboxAdsAPIsOverride,FencedFrames,
                SharedStorageAPI,FencedFrames:implementation_type/mparch''',
            '--enable-privacy-sandbox-ads-apis',
            '--expose-internals-for-testing',
            '--disk-cache-dir=/dev/null',
            '--media-cache-size=0',
            '--disk-cache-size=0',
        ],
        cache_temperature=cache_temperature_module.COLD)

  def RunNavigateSteps(self, action_runner):
    super(PageWithFrame, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript('''
        (function() {
          LoadFrame({{ type }}, {{ frame_url }});
        })();''',
                                    type=self.frame_type,
                                    frame_url=self.frame_url)
    action_runner.Wait(3)

  def RunPageInteractions(self, action_runner):
    pass


class IframePageSet(story.StorySet):
  """Various ad pages loaded into iframes"""

  def __init__(self):
    super(IframePageSet,
          self).__init__(archive_data_file='data/ad_frame.json',
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for url in URL_LIST:
      self.AddStory(PageWithFrame('iframe', url, self))


class FencedFramePageSet(story.StorySet):
  """Various ad pages loaded into iframes"""

  def __init__(self):
    super(FencedFramePageSet,
          self).__init__(archive_data_file='data/ad_frame.json',
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for url in URL_LIST:
      self.AddStory(PageWithFrame('fencedframe', url, self))
