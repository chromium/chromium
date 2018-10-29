# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import AndroidOnly
from decorators import NotAndroid
from decorators import ChromeVersionEqualOrAfterM

from selenium.common.exceptions import TimeoutException

class SafeBrowsing(IntegrationTest):

  @AndroidOnly
  def testSafeBrowsingOn(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')

      # Starting in M63 LoadURL will timeout when the safebrowsing
      # interstitial appears.
      try:
        t.LoadURL('http://testsafebrowsing.appspot.com/s/malware.html')
        responses = t.GetHTTPResponses()
        self.assertEqual(0, len(responses))
      except TimeoutException:
        pass

  @AndroidOnly
  @ChromeVersionEqualOrAfterM(72)
  def testSafeBrowsingMalwareWithOnDeviceChecksOn(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg(
          '--force-fieldtrial-params='
          'DataReductionProxyServerExperiments.'
          'OnDeviceSafeBrowsingFieldTrialEnabled:'
          'exp/disable_server_safebrowsing')
      t.AddChromeArg(
          '--force-fieldtrials=DataReductionProxyServerExperiments/'
          'OnDeviceSafeBrowsingFieldTrialEnabled')

      # Starting in M63 LoadURL will timeout when the safebrowsing
      # interstitial appears.
      try:
        t.LoadURL('http://testsafebrowsing.appspot.com/s/malware.html')
        responses = t.GetHTTPResponses()
        self.assertEqual(0, len(responses))
      except TimeoutException:
        # Verify that on device safebrowsing records unsafe for mainframe
        # request at bucket=0
        unsafe_resources = t.GetHistogram('SB2.ResourceTypes2.Unsafe')
        self.assertEqual(1, unsafe_resources['count'])
        self.assertEqual(1, unsafe_resources['buckets'][0]['count'])

  @NotAndroid
  def testSafeBrowsingOff(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://testsafebrowsing.appspot.com/s/malware.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(1, len(responses))
      for response in responses:
        self.assertHasChromeProxyViaHeader(response)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
