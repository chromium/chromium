# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
import time
from common import TestDriver
from common import IntegrationTest
from decorators import NotAndroid
from decorators import ChromeVersionBeforeM
from decorators import ChromeVersionEqualOrAfterM
import json

class Smoke(IntegrationTest):

  # Ensure Chrome does not use DataSaver in Incognito mode.
  # Clank does not honor the --incognito flag.
  @NotAndroid
  def testCheckPageWithIncognito(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--incognito')
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

  # Ensure Chrome does not use DataSaver when holdback is enabled.
  @ChromeVersionBeforeM(74)
  def testCheckPageWithHoldback(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--force-fieldtrials=DataCompressionProxyHoldback/'
                               'Enabled')
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      num_chrome_proxy_request_headers = 0
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)
        if ('chrome-proxy' in response.request_headers):
          num_chrome_proxy_request_headers += 1
      # DataSaver histograms must still be logged.
      t.SleepUntilHistogramHasEntry('PageLoad.Clients.DataReductionProxy.'
              'ParseTiming.NavigationToParseStart')
      self.assertEqual(num_chrome_proxy_request_headers, 0)
      # Ensure that Chrome did not attempt to use DataSaver and got a bypass.
      histogram = t.GetHistogram('DataReductionProxy.BypassedBytes.'
        'Status502HttpBadGateway', 5)
      self.assertEqual(histogram, {})
      histogram = t.GetHistogram('DataReductionProxy.BlockTypePrimary', 5)
      self.assertEqual(histogram, {})
      histogram = t.GetHistogram('DataReductionProxy.BypassTypePrimary', 5)
      self.assertEqual(histogram, {})

  # Ensure Chrome uses DataSaver in normal mode.
  def testCheckPageWithNormalMode(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      num_chrome_proxy_request_headers = 0
      for response in responses:
        self.assertHasProxyHeaders(response)
        if ('chrome-proxy' in response.request_headers):
          num_chrome_proxy_request_headers += 1
      t.SleepUntilHistogramHasEntry('PageLoad.Clients.DataReductionProxy.'
        'ParseTiming.NavigationToParseStart')
      self.assertGreater(num_chrome_proxy_request_headers, 0)

  # Ensure pageload metric pingback with DataSaver.
  @ChromeVersionBeforeM(79)
  def testPingback(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--enable-data-reduction-proxy-force-pingback')
      t.LoadURL('http://check.googlezip.net/test.html')
      t.LoadURL('http://check.googlezip.net/test.html')
      t.SleepUntilHistogramHasEntry("DataReductionProxy.Pingback.Succeeded")
      t.SleepUntilHistogramHasEntry("DataReductionProxy.Pingback.Attempted")
      # Verify one pingback attempt that was successful.
      attempted = t.GetBrowserHistogram('DataReductionProxy.Pingback.Attempted')
      self.assertEqual(1, attempted['count'])
      succeeded = t.GetBrowserHistogram('DataReductionProxy.Pingback.Succeeded')
      self.assertEqual(1, succeeded['count'])

  # Ensure pageload metric pingback with DataSaver has the variations header.
  @ChromeVersionEqualOrAfterM(62)
  @ChromeVersionBeforeM(79)
  def testPingbackHasVariations(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--enable-data-reduction-proxy-force-pingback')
      t.UseNetLog()
      # Force set the variations ID, so they are send along with the pingback
      # request.
      t.AddChromeArg('--force-variation-ids=42')
      t.LoadURL('http://check.googlezip.net/test.html')
      t.LoadURL('http://check.googlezip.net/test.html')
      t.SleepUntilHistogramHasEntry("DataReductionProxy.Pingback.Succeeded")

      # Look for the request made to data saver pingback server.
      data = t.StopAndGetNetLog()
      variation_header_count = 0
      for i in data["events"]:
        dumped_event = json.dumps(i)
        if dumped_event.find("datasaver.googleapis.com") !=-1 and\
          dumped_event.find("recordPageloadMetrics") != -1 and\
          dumped_event.find("headers") != -1 and\
          dumped_event.find("accept-encoding") != -1 and\
          dumped_event.find("x-client-data") !=-1:
            variation_header_count = variation_header_count + 1

      # Variation IDs are set. x-client-data should be present in the request
      # headers.
      self.assertLessEqual(1, variation_header_count)

  # Verify unique page IDs are sent in the Chrome-Proxy header.
  @ChromeVersionEqualOrAfterM(59)
  def testPageID(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      page_identifiers = []
      page_loads = 5

      for i in range (0, page_loads):
        t.LoadURL('http://check.googlezip.net/test.html')
        responses = t.GetHTTPResponses()
        self.assertEqual(2, len(responses))
        pid_in_page_count = 0
        page_id = ''
        for response in responses:
          if not response.request_headers:
            continue
          self.assertHasProxyHeaders(response)
          self.assertEqual(200, response.status)
          chrome_proxy_header = response.request_headers['chrome-proxy']
          chrome_proxy_directives = chrome_proxy_header.split(',')
          for directive in chrome_proxy_directives:
            if 'pid=' in directive:
              pid_in_page_count = pid_in_page_count+1
              page_id = directive.split('=')[1]
              self.assertNotEqual('', page_id)
              self.assertNotIn(page_id, page_identifiers)
        page_identifiers.append(page_id)
        self.assertEqual(1, pid_in_page_count)

  # Ensure that block causes resources to load from the origin directly.
  def testCheckBlockIsWorking(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/block')
      responses = t.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

  # Ensure image resources are compressed.
  def testCheckImageIsCompressed(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/static')
      # http://check.googlezip.net/static is a test page that has
      # image resources.
      responses = t.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
