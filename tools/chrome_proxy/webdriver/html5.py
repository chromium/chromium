# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest


class HTML5(IntegrationTest):

  # This test site has a div with id="pointsPanel" that is rendered if the
  # browser is capable of using HTML5.
  def testHTML5(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://html5test.com/')
      t.WaitForJavascriptExpression(
        'document.getElementsByClassName("pointsPanel")', 15)
      checked_main_page = False
      for response in t.GetHTTPResponses():
        # Site has a lot on it, just check the main page.
        if (response.url == 'http://html5test.com/'
            or response.url == 'http://html5test.com/index.html'):
          self.assertHasProxyHeaders(response)
          checked_main_page = True
      if not checked_main_page:
        self.fail("Did not check any page!")
if __name__ == '__main__':
  IntegrationTest.RunAllTests()
