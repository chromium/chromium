# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest

class Fallback(IntegrationTest):

  # Ensure that when a carrier blocks using the secure proxy, requests fallback
  # to the HTTP proxy server.
  def testSecureProxyProbeFallback(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      # Set the secure proxy check URL to the google.com favicon, which will be
      # interpreted as a secure proxy check failure since the response body is
      # not "OK". The google.com favicon is used because it will load reliably
      # fast, and there have been problems with chromeproxy-test.appspot.com
      # being slow and causing tests to flake.
      test_driver.AddChromeArg(
          '--data-reduction-proxy-secure-proxy-check-url='
          'http://www.google.com/favicon.ico')

      # Start chrome to begin the secure proxy check
      test_driver.LoadURL('about:blank')

      self.assertTrue(
        test_driver.SleepUntilHistogramHasEntry("DataReductionProxy.ProbeURL"))

      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      # Verify that DataReductionProxy.ProbeURL histogram has one entry in
      # FAILED_PROXY_DISABLED, which is bucket=1.
      histogram = test_driver.GetBrowserHistogram('DataReductionProxy.ProbeURL')
      self.assertGreaterEqual(histogram['count'], 1)
      self.assertGreaterEqual(histogram['buckets'][0]['low'], 1)
      for response in responses:
          self.assertHasProxyHeaders(response)
          # TODO(rajendrant): Fix the correct protocol received.
          # self.assertEqual(u'http/2+quic/43', response.protocol)

  # DataSaver uses a https proxy by default, if that fails it will fall back to
  # a http proxy; and if that fails, it will fall back to a direct connection
  def testHTTPToDirectFallback(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--data-reduction-proxy-http-proxies='
                               'http://nonexistent.googlezip.net;'
                               'http://compress.googlezip.net')

      test_driver.LoadURL('http://check.googlezip.net/fallback/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertEqual(80, response.port)

      test_driver.LoadURL('http://check.googlezip.net/block/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
