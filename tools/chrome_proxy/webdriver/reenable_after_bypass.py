# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import common
from common import TestDriver
from common import IntegrationTest
from decorators import Slow

class ReenableAfterBypass(IntegrationTest):
  """Tests for ensuring that DRPs are reenabled after bypasses expire.

  These tests take a very long time to run since they wait for their respective
  bypasses to expire. These tests have been separated out into their own file in
  order to make it easier to run these tests separately from the others.
  """

  # Verify that longer bypasses triggered by the Data Reduction Proxy only last
  # as long as they're supposed to, and that the proxy is used once again after
  # the bypass has ended.
  @Slow
  def testReenableAfterSetBypass(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      # Load URL that triggers a 20-second bypass of all proxies.
      test_driver.LoadURL('http://check.googlezip.net/block20/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

      # Verify that the Data Reduction Proxy is still bypassed.
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

      # Verify that the Data Reduction Proxy is no longer bypassed after 20
      # seconds.
      time.sleep(20)
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

  # Verify that when the Data Reduction Proxy responds with the "block=0"
  # directive, Chrome bypasses all proxies for the next 1-5 minutes.
  @Slow
  def testReenableAfterBypass(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      # Load URL that triggers a bypass of all proxies that lasts between 1 and
      # 5 minutes.
      test_driver.LoadURL('http://check.googlezip.net/block/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

      # Verify that the Data Reduction Proxy is still bypassed after 30 seconds.
      time.sleep(30)
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

      # Verify that the Data Reduction Proxy is no longer bypassed 5 minutes
      # after the original bypass was triggered.
      time.sleep(60 * 4 + 30)
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)


if __name__ == '__main__':
  IntegrationTest.RunAllTests()
