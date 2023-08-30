# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

URL_LIST = [
    "https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B9340305.127562781;sz=970x250",
    "https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B9340305.128470354;sz=300x600",
    "https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B8455269.126839257;sz=970x250",
    "https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B9340305.127461685;sz=970x250",
    "https://ad.doubleclick.net/ddm/adi/N378.3159.GOOGLE3/B9340305.138620671;sz=970x250",
    "https://ad.doubleclick.net/ddm/adi/N378.275220.MYBESTOPTION.IT4/B9340305.128710365;sz=970x250",
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
                SharedStorageAPI,FencedFrames:implementation_type/mparch,
                FencedFramesDefaultMode''',
            '--enable-privacy-sandbox-ads-apis',
            '--expose-internals-for-testing',
            '--disk-cache-dir=/dev/null',
        ],
        cache_temperature=cache_temperature_module.COLD)
    # We are loading a local file, which causes the page to mark itself as
    # local. This results in the test not downloading the WPR files we need.
    # Manually override the value so that it doesn't skip that step.
    self._is_local = False

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
