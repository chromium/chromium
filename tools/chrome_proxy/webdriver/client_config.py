# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM
from decorators import SkipIfForcedBrowserArg
import json


class ClientConfig(IntegrationTest):
  # Ensure client config is fetched at the start of the Chrome session, and the
  # session ID is correctly set in the chrome-proxy request header.
  def testClientConfig(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.SleepUntilHistogramHasEntry(
        'DataReductionProxy.ConfigService.FetchResponseCode')
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        # Verify that the proxy server honored the session ID.
        self.assertHasProxyHeaders(response)
        self.assertEqual(200, response.status)

  # Ensure Chrome uses a direct connection when no valid client config is given.
  @SkipIfForcedBrowserArg('data-reduction-proxy-config-url')
  def testNoClientConfigUseDirect(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      # The test server won't respond with a valid client config.
      t.UseNetLog()
      t.AddChromeArg('--data-reduction-proxy-config-url='
        'https://chromeproxy-test.appspot.com')
      t.SleepUntilHistogramHasEntry(
        'DataReductionProxy.ConfigService.FetchResponseCode')
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

  # Ensure client config is fetched at the start of the Chrome session, and the
  # variations ID is set in the request.
  # Disabled on android because the net log is not copied yet. crbug.com/761507
  @ChromeVersionEqualOrAfterM(62)
  def testClientConfigVariationsHeader(self):
    with TestDriver() as t:
      t.UseNetLog()
      t.AddChromeArg('--enable-spdy-proxy-auth')
      # Force set the variations ID, so they are send along with the client
      # config fetch request.
      t.AddChromeArg('--force-variation-ids=42')

      t.LoadURL('http://check.googlezip.net/test.html')

      variation_header_count = 0

      # Look for the request made to data saver client config server.
      data = t.StopAndGetNetLog()
      for i in data["events"]:
        dumped_event = json.dumps(i)
        if dumped_event.find("datasaver.") !=-1 and\
          dumped_event.find(".googleapis.com") !=-1 and\
          dumped_event.find("clientConfigs") != -1 and\
          dumped_event.find("headers") != -1 and\
          dumped_event.find("accept-encoding") != -1 and\
          dumped_event.find("x-client-data") !=-1:
            variation_header_count = variation_header_count + 1

      # Variation IDs are set. x-client-data should be present in the request
      # headers.
      self.assertLessEqual(1, variation_header_count)

  # Ensure client config is fetched at the start of the Chrome session, and the
  # variations ID is not set in the request.
  # Disabled on android because the net log is not copied yet. crbug.com/761507
  @ChromeVersionEqualOrAfterM(62)
  def testClientConfigNoVariationsHeader(self):
    with TestDriver() as t:
      t.UseNetLog()
      t.AddChromeArg('--enable-spdy-proxy-auth')

      t.LoadURL('http://check.googlezip.net/test.html')

      variation_header_count = 0

      # Look for the request made to data saver client config server.
      data = t.StopAndGetNetLog()
      for i in data["events"]:
        dumped_event = json.dumps(i)
        if dumped_event.find("datasaver.") !=-1 and\
          dumped_event.find(".googleapis.com") !=-1 and\
          dumped_event.find("clientConfigs") != -1 and\
          dumped_event.find("headers") != -1 and\
          dumped_event.find("accept-encoding") != -1 and\
          dumped_event.find("x-client-data") !=-1:
            variation_header_count = variation_header_count + 1

      # Variation IDs are not set. x-client-data should not be present in the
      # request headers.
      self.assertEqual(0, variation_header_count)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
