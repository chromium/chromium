# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM


class Quic(IntegrationTest):

  # Ensure Chrome uses DataSaver when QUIC is enabled. This test should pass
  # even if QUIC is disabled on the server side. In that case, Chrome should
  # fallback to using the non-QUIC proxies.
  def testCheckPageWithQuicProxy(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')

      # Enable QUIC (including for non-core HTTPS proxies).
      t.AddChromeArg('--enable-quic')
      t.AddChromeArg('--force-fieldtrials=DataReductionProxyUseQuic/Enabled')
      t.AddChromeArg('--force-fieldtrial-params='
        'DataReductionProxyUseQuic.Enabled:enable_quic_non_core_proxies/true')
      # Enable usage of QUIC for non-core proxies via switch for older versions
      # of Chrome (M-59 and prior).
      t.AddChromeArg('--data-reduction-proxy-enable-quic-on-non-core-proxies')

      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

  # Ensure Chrome uses QUIC DataSaver proxy when QUIC is enabled. This test
  # may fail if QUIC is disabled on the server side.
  @ChromeVersionEqualOrAfterM(76)
  def testCheckPageWithQuicProxyTransaction(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')

      # Enable QUIC (including for non-core HTTPS proxies).
      t.AddChromeArg('--enable-quic')
      t.AddChromeArg('--force-fieldtrials=DataReductionProxyUseQuic/Enabled')
      t.AddChromeArg('--force-fieldtrial-params='
        'DataReductionProxyUseQuic.Enabled:enable_quic_non_core_proxies/true')
      # Enable usage of QUIC for non-core proxies via switch for older versions
      # of Chrome (M-59 and prior).
      t.AddChromeArg('--data-reduction-proxy-enable-quic-on-non-core-proxies')

      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)
      t.SleepUntilHistogramHasEntry('PageLoad.Clients.DataReductionProxy.'
        'ParseTiming.NavigationToParseStart')

      # Verify that histogram DataReductionProxy.Quic.ProxyStatus has at least 1
      # sample. This sample must be in bucket 0 (QUIC_PROXY_STATUS_AVAILABLE).
      proxy_status = t.GetHistogram('DataReductionProxy.Quic.ProxyStatus')
      self.assertLessEqual(1, proxy_status['count'])
      self.assertEqual(0, proxy_status['sum'])

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
