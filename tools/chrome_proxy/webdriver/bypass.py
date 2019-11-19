# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM


class Bypass(IntegrationTest):

  # Ensure Chrome does not use Data Saver for block-once, but does use Data
  # Saver for a subsequent request.
  def testBlockOnce(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/blocksingle/')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        if response.url == "http://check.googlezip.net/image.png":
          self.assertHasProxyHeaders(response)
        else:
          self.assertNotHasChromeProxyViaHeader(response)

  # Ensure Chrome does not use Data Saver for block=0, which uses the default
  # proxy retry delay.
  def testBypass(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/block/')
      for response in t.GetHTTPResponses():
        self.assertNotHasChromeProxyViaHeader(response)

      # Load another page and check that Data Saver is not used.
      t.LoadURL('http://check.googlezip.net/test.html')
      for response in t.GetHTTPResponses():
        self.assertNotHasChromeProxyViaHeader(response)

  # Ensure Chrome does not use Data Saver for HTTPS requests.
  def testHttpsBypass(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')

      # Load HTTP page and check that Data Saver is used.
      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

      # Load HTTPS page and check that Data Saver is not used.
      t.LoadURL('https://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

  # Verify that CORS requests receive a block-once from the data reduction
  # proxy by checking that those requests are retried without data reduction
  # proxy. CORS tests needs to be verified with and without OutOfBlinkCors
  # feature, since this feature affects sending CORS blocked response headers to
  # the renderer in different ways.
  def testCorsBypass(self):
    self.VerifyCorsTestWithOutOfBlinkCors(True)

  def testCorsBypassWithoutOutOfBlinkCors(self):
    # Verifies CORS behavior without OutOfBlinkCors feature. This feature is
    # currently under experimentation and once it is fully enabled this test can
    # be removed.
    self.VerifyCorsTestWithOutOfBlinkCors(False)

  def VerifyProxyServesPageWithoutBypass(self, test_driver):
    drp_responses = 0
    test_driver.LoadURL('http://check.googlezip.net/test.html')
    for response in test_driver.GetHTTPResponses():
      self.assertHasProxyHeaders(response)
      drp_responses += 1
    self.assertNotEqual(0, drp_responses)
    test_driver.SleepUntilHistogramHasEntry('PageLoad.Clients.'
              'DataReductionProxy.ParseTiming.NavigationToFirstContentfulPaint')
    self.assertEqual({},
      test_driver.GetHistogram('DataReductionProxy.BlockTypePrimary'))
    self.assertEqual({},
      test_driver.GetHistogram('DataReductionProxy.BlockTypeFallback'))
    self.assertEqual({},
      test_driver.GetHistogram('DataReductionProxy.BypassTypePrimary'))
    self.assertEqual({},
      test_driver.GetHistogram('DataReductionProxy.BypassTypeFallback'))

  def VerifyCorsTestWithOutOfBlinkCors(self, is_out_of_blink_cors_feature_on):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      if is_out_of_blink_cors_feature_on:
        test_driver.EnableChromeFeature('OutOfBlinkCors')
      else:
        test_driver.DisableChromeFeature('OutOfBlinkCors')

      # The CORS test page makes a cross-origin XHR request to a resource for
      # which DRP requests to bypass proxy for the current request. This 502
      # block-once bypass will not be received by the DRP bypass logic in the
      # renderer if proper response headers (Access-Control-Allow-Origin and
      # Access-Control-Allow-Headers) are not present.
      # This test verifies that the bypass logic received one block-once bypass,
      # and the request is retried without DRP. The 502 bypass response cannot
      # be verified to contain proper response headers set by the DRP since only
      # the retried response will be picked up by the webdriver.
      test_driver.LoadURL('http://www.gstatic.com/chrome/googlezip/cors/')

      test_driver.SleepUntilHistogramHasEntry(
        'DataReductionProxy.BlockTypePrimary')
      # Verify that one request received block-once(bucket=0), and no other
      # bypasses or fallbacks are received. Explicit checks for response headers
      # content-type=text/plain, Access-Control-Allow-Origin,
      # Access-Control-Allow-Headers, Via, Chrome-Proxy cannot be added, since
      # webdriver does not get the headers for 502 response. However, since
      # BlockTypePrimary is checked for one block-once entry, we know the DRP
      # bypass logic has picked it up.
      blocked = test_driver.GetHistogram('DataReductionProxy.BlockTypePrimary')
      self.assertEqual(1, blocked['count'])
      self.assertEqual(blocked['buckets'][0]['low'], 0)
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BlockTypeFallback'))
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BypassTypePrimary'))
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BypassTypeFallback'))
      cors_requests = 0
      same_origin_requests = 0
      for response in test_driver.GetHTTPResponses():
        # The cross-origin XHR request is a CORS request.
        if response.request_type == 'XHR':
          self.assertNotHasChromeProxyViaHeader(response)
          self.assertEqual(200, response.status)
          cors_requests = cors_requests + 1
        else:
          self.assertHasProxyHeaders(response)
          same_origin_requests = same_origin_requests + 1
      # Verify that both CORS and same origin requests were seen.
      self.assertNotEqual(0, same_origin_requests)
      self.assertNotEqual(0, cors_requests)

      # Navigate to a different page to verify that later requests are not
      # blocked.
      self.VerifyProxyServesPageWithoutBypass(test_driver)

  # Tests that data reduction proxy bypasses are not blocked by CORB. Since the
  # bypass/fallback handling is in the renderer process, CORB failures will skip
  # the bypasses/fallbacks and the resource will not be retried without data
  # reduction proxy.
  def testBypassNotBlockedByCorb(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      # The CORB test page loads an <img> to a cross-origin resource for which
      # DRP requests to bypass proxy for the current request. This 502
      # block-once bypass will not be received by the DRP bypass logic in the
      # renderer, if CORB blocks it based on mislabeled content-type or the
      # actual type observed from sniffing the body.
      test_driver.LoadURL('http://www.gstatic.com/chrome/googlezip/corb.html')
      drp_responses = 0
      for response in test_driver.GetHTTPResponses():
          if response.ResponseHasViaHeader():
            self.assertHasProxyHeaders(response)
            drp_responses += 1
      self.assertNotEqual(0, drp_responses)
      test_driver.SleepUntilHistogramHasEntry(
        'DataReductionProxy.BlockTypePrimary')
      # Verify that one request received block-once (bucket=0), and no other
      # bypasses or fallbacks are received.
      blocked = test_driver.GetHistogram('DataReductionProxy.BlockTypePrimary')
      self.assertEqual(1, blocked['count'])
      self.assertEqual(blocked['buckets'][0]['low'], 0)
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BlockTypeFallback'))
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BypassTypePrimary'))
      self.assertEqual({},
        test_driver.GetHistogram('DataReductionProxy.BypassTypeFallback'))

      # Navigate to a different page to verify that later requests are not
      # blocked.
      self.VerifyProxyServesPageWithoutBypass(test_driver)

  # Verify that when an origin times out using Data Saver, the request is
  # fetched directly and data saver is bypassed only for one request.
  def testOriginTimeoutBlockOnce(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      # Load URL that times out when the proxy server tries to access it.
      test_driver.LoadURL('http://chromeproxy-test.appspot.com/blackhole')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
          self.assertNotHasChromeProxyViaHeader(response)

      # Load HTTP page and check that Data Saver is used.
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

  # Verify that Chrome does not bypass the proxy when a response gets a missing
  # via header.
  @ChromeVersionEqualOrAfterM(67)
  def testMissingViaHeaderNoBypassExperiment(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.EnableChromeFeature('DataReductionProxyRobustConnection'
        '<DataReductionProxyRobustConnection')
      t.AddChromeArg('--force-fieldtrials=DataReductionProxyRobustConnection/'
        'Enabled')
      t.AddChromeArg('--force-fieldtrial-params='
        'DataReductionProxyRobustConnection.Enabled:'
        'warmup_fetch_callback_enabled/true/'
        'bypass_missing_via_disabled/true')
      t.AddChromeArg('--disable-data-reduction-proxy-warmup-url-fetch')
      t.AddChromeArg('--data-reduction-proxy-http-proxies='
        # The chromeproxy-test server is a simple HTTP server. If it is served a
        # proxy-request, it will respond with a 404 error page. It will not set
        # the Via header on the response.
        'https://chromeproxy-test.appspot.com;http://compress.googlezip.net')

      # Loading this URL should not hit the actual check.googlezip.net origin.
      # Instead, the test server proxy should fully handle the request and will
      # respond with an error page.
      t.LoadURL("http://check.googlezip.net/test.html")
      for response in t.GetHTTPResponses():
        self.assertNotHasChromeProxyViaHeader(response)

      # Check the via bypass histograms are empty.
      histogram = t.GetHistogram(
        'DataReductionProxy.BypassedBytes.MissingViaHeader4xx')
      self.assertEqual(0, len(histogram))
      histogram = t.GetHistogram(
        'DataReductionProxy.BypassedBytes.MissingViaHeaderOther')
      self.assertEqual(0, len(histogram))

      # Check that the fetch used the proxy.
      histogram = t.GetHistogram('DataReductionProxy.ProxySchemeUsed')
      self.assertEqual(histogram['buckets'][0]['low'], 2)
      self.assertEqual(histogram['buckets'][0]['high'], 3)

  # Verify that the Data Reduction Proxy understands the "exp" directive.
  def testExpDirectiveBypass(self):
    # If it was attempted to run with another experiment, skip this test.
    if common.ParseFlags().browser_args and ('--data-reduction-proxy-experiment'
        in common.ParseFlags().browser_args):
      self.skipTest('This test cannot be run with other experiments.')
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.SetExperiment('client_test_bypass')

      # Verify that loading a page other than the specific exp directive test
      # page loads through the proxy without being bypassed.
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

      # Verify that loading the exp directive test page with
      # "exp=client_test_bypass" triggers a bypass.
      test_driver.LoadURL('http://check.googlezip.net/exp/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)

    # Verify that loading the same test page without setting
    #{ }"exp=client_test_bypass" loads through the proxy without being bypassed.
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')

      test_driver.LoadURL('http://check.googlezip.net/exp/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

  # Data Saver uses a HTTPS proxy by default, if that fails it will fall back to
  # a HTTP proxy.
  def testBadHTTPSFallback(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--data-reduction-proxy-http-proxies='
                               'http://compress.googlezip.net')

      test_driver.LoadURL('http://check.googlezip.net/fallback/')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertEqual(80, response.port)

  # Get the client type with the first request, then check bypass on the
  # appropriate test page
  def testClientTypeBypass(self):
    clientType = ''
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      # Page that should not bypass.
      test_driver.LoadURL('http://check.googlezip.net/test.html')
      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(0, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)
        if 'chrome-proxy' in response.request_headers:
            chrome_proxy_header = response.request_headers['chrome-proxy']
            chrome_proxy_directives = chrome_proxy_header.split(',')
            for directive in chrome_proxy_directives:
                if 'c=' in directive:
                    clientType = directive[3:]
    self.assertTrue(clientType)

    clients = ['android', 'webview', 'ios', 'linux', 'win', 'chromeos']
    for client in clients:
      with TestDriver() as test_driver:
        test_driver.LoadURL('http://check.googlezip.net/chrome-proxy-header/'
                          'c_%s/' %client)
        responses = test_driver.GetHTTPResponses()
        self.assertEqual(2, len(responses))
        for response in responses:
          if client in clientType:
            self.assertNotHasChromeProxyViaHeader(response)

  def testHTTPSubresourcesOnHTTPSPage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.LoadURL(
        'https://check.googlezip.net/previews/mixed_images.html')
      responses = test_driver.GetHTTPResponses()
      self.assertEqual(3, len(responses))
      for response in responses:
        if response.url.startswith('http://'):
          self.assertHasProxyHeaders(response)
        elif response.url.startswith('https://'):
          self.assertNotHasChromeProxyViaHeader(response)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
