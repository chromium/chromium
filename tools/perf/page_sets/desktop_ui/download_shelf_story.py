# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page

DOWNLOAD_URL = 'https://dl.google.com/chrome/mac/stable/GGRO/googlechrome.dmg'


class DownloadShelfStory(page.Page):
  """Base class for stories to download files"""

  def __init__(self, story_set, extra_browser_args=None):
    super(DownloadShelfStory,
          self).__init__(url=self.URL,
                         name=self.NAME,
                         page_set=story_set,
                         extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    url_list = self.URL_LIST
    tabs = action_runner.tab.browser.tabs
    for url in url_list:
      # Suppress error caused by tab closed before it returns occasionally
      try:
        tabs.New(url=url)
      except Exception:
        pass

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(10)


class DownloadShelfStory1File(DownloadShelfStory):
  NAME = 'download_shelf:1file'
  URL_LIST = [DOWNLOAD_URL]
  URL = URL_LIST[0]


class DownloadShelfStory5File(DownloadShelfStory):
  NAME = 'download_shelf:5file'
  URL_LIST = [DOWNLOAD_URL] * 5
  URL = URL_LIST[0]
