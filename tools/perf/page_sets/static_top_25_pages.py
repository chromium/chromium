# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import shared_page_state
from telemetry import story

STATIC_TOP_25_DIR = 'static_top_25'

class StaticTop25PageSet(story.StorySet):

  """ Page set consists of static top 25 pages. """

  def __init__(self):
    super(StaticTop25PageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)

    shared_desktop_state = shared_page_state.SharedDesktopPageState
    files = [
        'amazon.html',
        'blogger.html',
        'booking.html',
        'cnn.html',
        'ebay.html',
        'espn.html',
        'facebook.html',
        'gmail.html',
        'googlecalendar.html',
        'googledocs.html',
        'google.html',
        'googleimagesearch.html',
        'googleplus.html',
        'linkedin.html',
        'pinterest.html',
        'techcrunch.html',
        'twitter.html',
        'weather.html',
        'wikipedia.html',
        'wordpress.html',
        'yahooanswers.html',
        'yahoogames.html',
        'yahoonews.html',
        'yahoosports.html',
        'youtube.html'
        ]
    for f in files:
        url = "file://%s/%s" % (STATIC_TOP_25_DIR, f)
        self.AddStory(
            page.Page(url, self, self.base_dir,
                      shared_page_state_class=shared_desktop_state,
                      name=url))
