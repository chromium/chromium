# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import ParseFlags
from common import TestDriver
from common import IntegrationTest
from decorators import AndroidOnly
from decorators import ChromeVersionBeforeM
from decorators import ChromeVersionEqualOrAfterM
from urlparse import urlparse

import time

# These are integration tests for server provided previews and the
# protocol that supports them.
class LitePage(IntegrationTest):

  # Verifies that a Lite Page is served for slow connection if any copyright
  # restricted country blacklist is ignored.
  # Note: this test is for the CPAT protocol change in M-61.
  @ChromeVersionEqualOrAfterM(61)
  def testLitePageWithoutCopyrightRestriction(self):
    # If it was attempted to run with another experiment, skip this test.
    if common.ParseFlags().browser_args and ('--data-reduction-proxy-experiment'
        in common.ParseFlags().browser_args):
      self.skipTest('This test cannot be run with other experiments.')
    with TestDriver() as test_driver:
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      test_driver.SetExperiment('ignore_preview_blacklist')
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--force-effective-connection-type=2G')

      test_driver.LoadURL('http://check.googlezip.net/test.html')

      lite_page_responses = 0
      checked_chrome_proxy_header = False
      for response in test_driver.GetHTTPResponses():
        if response.request_headers:
          # Verify client sends ignore directive on main frame request.
          self.assertIn('exp=ignore_preview_blacklist',
            response.request_headers['chrome-proxy'])
          self.assertEqual('2G', response.request_headers['chrome-proxy-ect'])
          checked_chrome_proxy_header = True
        if response.url.endswith('html'):
          self.assertTrue(self.checkLitePageResponse(response))
          lite_page_responses = lite_page_responses + 1
          # Expect no fallback page policy
          if 'chrome-proxy' in response.response_headers:
            self.assertNotIn('page-policies',
                             response.response_headers['chrome-proxy'])
        else:
          # No subresources should accept transforms.
          self.assertNotIn('chrome-proxy-accept-transform',
            response.request_headers)
      self.assertTrue(checked_chrome_proxy_header)

      # Verify that a Lite Page response for the main frame was seen.
      self.assertEqual(1, lite_page_responses)

      self.assertPreviewShownViaHistogram(test_driver, 'LitePage')

  # Checks that a Nano Lite Page does not have an error when scrolling to the
  # bottom of the page and is able to load all resources. Nano pages don't
  # request additional resources when scrolling. This test is only run on
  # Android because it depends on window size of the browser.
  @AndroidOnly
  @ChromeVersionEqualOrAfterM(65)
  def testLitePageNano(self):
    # If it was attempted to run with another experiment, skip this test.
    if common.ParseFlags().browser_args and ('--data-reduction-proxy-experiment'
        in common.ParseFlags().browser_args):
      self.skipTest('This test cannot be run with other experiments.')
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      # Need to force 2G speed to get lite-page response.
      test_driver.AddChromeArg('--force-effective-connection-type=2G')
      # Set exp=client_test_nano to force Nano response.
      test_driver.SetExperiment('client_test_nano')

      # This page is long and has many media resources.
      test_driver.LoadURL('http://check.googlezip.net/metrics/index.html')
      time.sleep(2)

      lite_page_responses = 0
      btf_response = 0
      image_responses = 0
      for response in test_driver.GetHTTPResponses():
        # Verify that a Lite Page response for the main frame was seen.
        if response.url.endswith('html'):
          if (self.checkLitePageResponse(response)):
             lite_page_responses = lite_page_responses + 1
        # Keep track of BTF responses.
        u = urlparse(response.url)
        if u.path == "/b":
          btf_response = btf_response + 1
        # Keep track of image responses.
        if response.url.startswith("data:image"):
          image_responses = image_responses + 1
        # Some video requests don't go through Flywheel.
        if 'content-type' in response.response_headers and ('video/mp4'
            in response.response_headers['content-type']):
          continue
        # Make sure non-video requests are proxied.
        self.assertHasProxyHeaders(response)
        # Make sure there are no 4XX or 5xx status codes.
        self.assertLess(response.status, 400)

      self.assertEqual(1, lite_page_responses)
      self.assertEqual(1, btf_response)
      self.assertGreater(1, image_responses)

  # Tests that the stale previews UI is shown on a stale Lite page.
  # The stale timestamp histogram is not logged with the new UI unless the page
  # info dialog is opened which can't be done in a ChromeDriver test.
  @AndroidOnly
  @ChromeVersionEqualOrAfterM(65)
  @ChromeVersionBeforeM(76)
  def testStaleLitePageNano(self):
    # If it was attempted to run with another experiment, skip this test.
    if common.ParseFlags().browser_args and ('--data-reduction-proxy-experiment'
        in common.ParseFlags().browser_args):
      self.skipTest('This test cannot be run with other experiments.')
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      test_driver.DisableChromeFeature('AndroidOmniboxPreviewsBadge')
      test_driver.AddChromeArg('--force-effective-connection-type=2G')
      # Set exp=client_test_nano to force Lite page response.
      test_driver.SetExperiment('client_test_nano')
      # LoadURL waits for onLoadFinish so the Previews UI will be showing by
      # then since it's triggered on commit.
      test_driver.LoadURL(
        'http://check.googlezip.net/cacheable/test.html?age_seconds=360')

      test_driver.SleepUntilHistogramHasEntry(
        'Previews.StalePreviewTimestampShown')
      histogram = test_driver.GetBrowserHistogram(
        'Previews.StalePreviewTimestampShown')
      self.assertEqual(1, histogram['count'])
      # Check that there is a single entry in the 'Timestamp Shown' bucket.
      self.assertEqual(
        {'count': 1, 'high': 1, 'low': 0},
        histogram['buckets'][0])

      # Go to a non stale page and check that the stale timestamp is not shown.
      test_driver.LoadURL(
        'http://check.googlezip.net/cacheable/test.html?age_seconds=0')

      histogram = test_driver.GetBrowserHistogram(
        'Previews.StalePreviewTimestampShown')
      # Check that there is still a single entry in the 'Timestamp Shown'
      # bucket.
      self.assertEqual(
        {'count': 1, 'high': 1, 'low': 0},
        histogram['buckets'][0])

  # Verifies Lo-Fi fallback via the page-policies server directive.
  # Note: this test is for the CPAT protocol change in M-61.
  @ChromeVersionEqualOrAfterM(61)
  @ChromeVersionBeforeM(74)
  def testLitePageFallbackViaPagePolicies(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature(
        'NetworkQualityEstimator<NetworkQualityEstimator')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/Slow2G')
      test_driver.AddChromeArg('--force-fieldtrials='
                               'NetworkQualityEstimator/Enabled/')

      test_driver.LoadURL('http://check.googlezip.net/lite-page-fallback')

      lite_page_responses = 0
      lofi_resource = 0
      for response in test_driver.GetHTTPResponses():
        self.assertEqual('Slow-2G',
                         response.request_headers['chrome-proxy-ect'])

        if response.url.endswith('html'):
          # Verify that the server provides the fallback directive
          self.assertIn('page-policies=empty-image',
                        response.response_headers['chrome-proxy'])
          # Main resource should not accept and transform to lite page.
          if self.checkLitePageResponse(response):
            lite_page_responses = lite_page_responses + 1
        if response.url.endswith('png'):
          if self.checkLoFiResponse(response, True):
            lofi_resource = lofi_resource + 1

      self.assertEqual(0, lite_page_responses)
      self.assertNotEqual(0, lofi_resource)

  # Checks that the server does not provide a preview (neither Lite Page nor
  # fallback to LoFi) for a fast connection.
  # Note: this test is for the CPAT protocol change in M-61.
  @ChromeVersionEqualOrAfterM(61)
  def testPreviewNotProvidedForFastConnection(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature(
        'NetworkQualityEstimator<NetworkQualityEstimator')
      test_driver.EnableChromeFeature('NetworkService')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/4G')
      test_driver.AddChromeArg(
          '--force-fieldtrials='
          'NetworkQualityEstimator/Enabled/'
          'DataReductionProxyPreviewsBlackListTransition/Enabled/')

      test_driver.LoadURL('http://check.googlezip.net/test.html')

      checked_chrome_proxy_header = False
      for response in test_driver.GetHTTPResponses():
        if response.request_headers:
          self.assertEqual('4G', response.request_headers['chrome-proxy-ect'])
          checked_chrome_proxy_header = True
        if response.url.endswith('html'):
          # Main resource should accept lite page but not be transformed.
          self.assertEqual('lite-page',
            response.request_headers['chrome-proxy-accept-transform'])
          self.assertNotIn('chrome-proxy-content-transform',
            response.response_headers)
          # Expect no fallback page policy
          if 'chrome-proxy' in response.response_headers:
            self.assertNotIn('page-policies',
                             response.response_headers['chrome-proxy'])
        else:
          # No subresources should accept transforms.
          self.assertNotIn('chrome-proxy-accept-transform',
            response.request_headers)

      self.assertPreviewNotShownViaHistogram(test_driver, 'LoFi')
      self.assertPreviewNotShownViaHistogram(test_driver, 'LitePage')
      self.assertTrue(checked_chrome_proxy_header)

  # Checks the default of whether server previews are enabled or not
  # based on whether running on Android (enabled) or not (disabled).
  # This is a regression test that the DataReductionProxyDecidesTransform
  # Feature is not enabled by default for non-Android platforms.
  @ChromeVersionEqualOrAfterM(64)
  def testDataReductionProxyDecidesTransformDefault(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature(
        'NetworkQualityEstimator<NetworkQualityEstimator')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/2G')
      test_driver.AddChromeArg(
          '--force-fieldtrials='
          'NetworkQualityEstimator/Enabled/'
          'DataReductionProxyPreviewsBlackListTransition/Enabled/')

      test_driver.LoadURL('http://check.googlezip.net/test.html')

      for response in test_driver.GetHTTPResponses():
        if not response.request_headers:
          continue
        self.assertEqual('2G', response.request_headers['chrome-proxy-ect'])
        if response.url.endswith('html'):
          if ParseFlags().android:
            # CPAT provided on Android
            self.assertIn('chrome-proxy-accept-transform',
              response.request_headers)
          else:
            # CPAT NOT provided on Desktop
            self.assertNotIn('chrome-proxy-accept-transform',
              response.request_headers)
            self.assertNotIn('chrome-proxy-content-transform',
              response.response_headers)
          continue

  # Checks that the server provides a working interactive CASPR.
  @ChromeVersionEqualOrAfterM(65)
  def testInteractiveCASPR(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      # Need to force 2G speed to get a preview.
      test_driver.AddChromeArg('--force-effective-connection-type=2G')
      # Set exp=client_test_icaspr to force iCASPR response.
      test_driver.SetExperiment('ihdp_integration')

      test_driver.LoadURL('http://check.googlezip.net/previews/ihdp.html')

      # The original page does not have any script resources (scripts are
      # inlined in the HTML). The snapshotted page should contain exactly one
      # script: the snapshot.
      num_scripts = 0
      for response in test_driver.GetHTTPResponses():
        if response.request_type == 'Script':
          num_scripts += 1

      self.assertEqual(1, num_scripts)

      # Make sure the snapshot is restored correctly.
      window_x = test_driver.ExecuteJavascriptStatement('window.x')
      self.assertEqual(10, window_x)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
