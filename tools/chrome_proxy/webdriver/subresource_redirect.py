# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM

LITEPAGES_REGEXP = r'https://\w+\.litepages\.googlezip\.net/.*'

class SubresourceRedirect(IntegrationTest):

  # Verifies that image subresources on a page have been returned
  # from the compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testCompressImage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('https://check.googlezip.net/static/index.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(5, image_responses)

  # Verifies that when the image compression server serves a
  # redirect, then Chrome fetches the image directly.
  @ChromeVersionEqualOrAfterM(77)
  def testOnRedirectImage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      # Image compression server returns a 307 for all images on this webpage.
      test_driver.LoadURL(
        'https://testsafebrowsing.appspot.com/s/image_small.html')

      server_bypass = 0
      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1
        if ('https://testsafebrowsing.appspot.com/s/bad_assets/small.png'
          == response.url and 200 == response.status):
          server_bypass += 1

      self.assertEqual(1, server_bypass)
      self.assertEqual(0, image_responses)

  # Verifies that non-image subresources aren't redirected to the
  # compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonImage(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('https://check.googlezip.net/testvideo.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

  # Verifies that non-secure connections aren't redirected to the
  # compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonHTTPS(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-subresource-redirect')
      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
