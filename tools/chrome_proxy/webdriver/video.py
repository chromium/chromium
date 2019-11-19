# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import common
from common import TestDriver
from common import IntegrationTest
from common import ParseFlags
from decorators import AndroidOnly
from decorators import Slow
from decorators import ChromeVersionEqualOrAfterM

from selenium.webdriver.common.by import By

class Video(IntegrationTest):

  # Returns the ofcl value in chrome-proxy header.
  def getChromeProxyOFCL(self, response):
    self.assertIn('chrome-proxy', response.response_headers)
    chrome_proxy_header = response.response_headers['chrome-proxy']
    self.assertIn('ofcl=', chrome_proxy_header)
    return chrome_proxy_header.split('ofcl=', 1)[1].split(',', 1)[0]

  # Check videos are proxied.
  def testCheckVideoHasViaHeader(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL(
        'http://check.googlezip.net/cacheable/video/buck_bunny_tiny.html')
      responses = t.GetHTTPResponses()
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertHasProxyHeaders(response)

  def testCheckVideoBypass(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL(
        'http://check.googlezip.net/blocksingle/blocksingle_embedded_video.html')
      saw_video_response = False
      for response in t.GetHTTPResponses():
        if 'video' in response.response_headers['content-type']:
          self.assertNotHasChromeProxyViaHeader(response)
          saw_video_response = True
        else:
          self.assertHasProxyHeaders(response)
      self.assertTrue(saw_video_response, 'No video request seen in test!')

  # Videos fetched via an XHR request should not be proxied.
  def testNoCompressionOnXHR(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      # The test will actually use Javascript, so use a site that won't have any
      # resources on it that could interfere.
      t.LoadURL('http://check.googlezip.net/connect')
      t.ExecuteJavascript(
        'var xhr = new XMLHttpRequest();'
        'xhr.open("GET", "/cacheable/video/data/buck_bunny_tiny.mp4", false);'
        'xhr.send();'
        'return;'
      )
      saw_video_response = False
      for response in t.GetHTTPResponses():
        if 'video' in response.response_headers['content-type']:
          self.assertNotHasChromeProxyViaHeader(response)
          saw_video_response = True
        else:
          self.assertHasProxyHeaders(response)
      self.assertTrue(saw_video_response, 'No video request seen in test!')

  @ChromeVersionEqualOrAfterM(64)
  def testRangeRequest(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://check.googlezip.net/report')
      time.sleep(2) # wait for page load
      t.ExecuteJavascript(
        'var xhr = new XMLHttpRequest();'
        'xhr.open("GET", "/metrics/local.png", false);'
        'xhr.setRequestHeader("Range", "bytes=0-2048");'
        'xhr.send();'
        'return;'
      )
      saw_range_response = False
      for response in t.GetHTTPResponses():
        self.assertHasProxyHeaders(response)
        if response.response_headers['status']=='206':
          saw_range_response = True
          content_range = response.response_headers['content-range']
          self.assertTrue(content_range.startswith('bytes 0-2048/'))
          compressed_full_content_length = int(content_range.split('/')[1])
          ofcl = int(self.getChromeProxyOFCL(response))
          # ofcl should be same as compressed full content length, since no
          # compression for XHR.
          self.assertEqual(ofcl, compressed_full_content_length)
      # Wait and navigate away to trigger the metrics recording for previous
      # page load.
      time.sleep(1)
      t.LoadURL('about:blank')
      original_kb_histogram = t.GetBrowserHistogram('PageLoad.Clients.'
        'DataReductionProxy.Experimental.Bytes.Network.Original2')
      compression_percent_histogram = t.GetBrowserHistogram('PageLoad.Clients.'
        'DataReductionProxy.Experimental.Bytes.Network.CompressionRatio2')
      self.assertEqual(1, original_kb_histogram['count'])
      self.assertEqual(1, compression_percent_histogram['count'])
      # Verify the total page size is 3 KB, and compression ratio.
      self.assertLessEqual(3, original_kb_histogram['sum'])
      self.assertEqual(compression_percent_histogram['sum'],
                       compressed_full_content_length/ofcl*100)
      self.assertTrue(saw_range_response, 'No range request was seen in test!')

  @ChromeVersionEqualOrAfterM(64)
  def testRangeRequestInVideo(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL(
        'http://check.googlezip.net/cacheable/video/buck_bunny_tiny.html')
      # Wait for the video to finish playing, plus some headroom.
      time.sleep(5)
      responses = t.GetHTTPResponses()
      saw_range_response = False
      for response in responses:
        self.assertHasProxyHeaders(response)
        if response.response_headers['status']=='206':
          saw_range_response = True
          content_range = response.response_headers['content-range']
          compressed_full_content_length = int(content_range.split('/')[1])
          ofcl = int(self.getChromeProxyOFCL(response))
          # ofcl should be greater than the compressed full content length.
          self.assertGreater(ofcl, compressed_full_content_length)
      self.assertTrue(saw_range_response, 'No range request was seen in test!')

  # Check the compressed video has the same frame count, width, height, and
  # duration as uncompressed.
  @Slow
  def testVideoMetrics(self):
    expected = {
      'duration': 3.068,
      'webkitDecodedFrameCount': 53.0,
      'videoWidth': 1280.0,
      'videoHeight': 720.0
    }
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL(
          'http://check.googlezip.net/cacheable/video/buck_bunny_tiny.html')
      # Check request was proxied and we got a compressed video back.
      for response in t.GetHTTPResponses():
        self.assertHasProxyHeaders(response)
        if ('content-type' in response.response_headers
            and 'video' in response.response_headers['content-type']):
          self.assertEqual('video/webm',
            response.response_headers['content-type'])
      if ParseFlags().android:
        t.FindElement(By.TAG_NAME, "video").click()
      else:
        t.ExecuteJavascriptStatement(
          'document.querySelectorAll("video")[0].play()')
      # Wait for the video to finish playing, plus some headroom.
      time.sleep(5)
      # Check each metric against its expected value.
      for metric in expected:
        actual = float(t.ExecuteJavascriptStatement(
          'document.querySelectorAll("video")[0].%s' % metric))
        self.assertAlmostEqual(expected[metric], actual, msg="Compressed video "
          "metric doesn't match expected! Metric=%s Expected=%f Actual=%f"
          % (metric, expected[metric], actual), places=None, delta=0.01)

  # Check that the compressed video can be seeked. Use a slow network to ensure
  # the entire video isn't downloaded before we have a chance to seek.
  @Slow
  @AndroidOnly
  def testVideoSeeking(self):
    with TestDriver(control_network_connection=True) as t:
      t.SetNetworkConnection("2G")
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL(
          'http://check.googlezip.net/cacheable/video/'+
          'buck_bunny_640x360_24fps.html')
      # Play, pause, seek to 1s before the end, play again.
      t.ExecuteJavascript(
        '''
        window.testDone = false;
        const v = document.getElementsByTagName("video")[0];
        let first = true;
        v.onplaying = function() {
          if (first) {
            v.pause();
            first = false;
          } else {
            window.testDone = true;
          }
        };
        v.onpause = function() {
          if (v.currentTime < v.duration) {
            v.currentTime = v.duration-1;
            v.play();
          }
        };
        v.play();
        ''')
      if ParseFlags().android:
        # v.play() won't work on Android, so give it a click instead.
        t.FindElement(By.TAG_NAME, "video").click()
      t.WaitForJavascriptExpression('window.testDone', 15)
      # Check request was proxied and we got a compressed video back.
      # We expect to make multiple requests for the video: ensure they
      # all have the same ETag.
      video_etag = None
      num_partial_requests = 0
      for response in t.GetHTTPResponses():
        self.assertHasProxyHeaders(response)
        rh = response.response_headers
        if ('content-type' in rh and 'video' in rh['content-type']):
          self.assertIn('etag', rh),
          self.assertEqual('video/webm', rh['content-type'])
          if video_etag == None:
            video_etag = rh['etag']
          else:
            self.assertEqual(video_etag, rh['etag'])
          if ('status' in rh and rh['status']=='206' and 'content-range' in rh
              and rh['content-range'].startswith('bytes ') and
              not rh['content-range'].startswith('bytes 0-')):
            num_partial_requests += 1
      # Also make sure that we had at least one partial Range request.
      self.assertGreaterEqual(num_partial_requests, 1)

  # Check the frames of a compressed video.
  @Slow
  def testVideoFrames(self):
    self.instrumentedVideoTest('http://check.googlezip.net/cacheable/video/buck_bunny_640x360_24fps_video.html')

  # Check the audio volume of a compressed video.
  #
  # This test makes some assumptions about the way audio is decoded and
  # processed in JavaScript on different platforms. Despite getting the same
  # video bytes from the proxy across all platforms, different data is generated
  # out of the window.AudioContext object. As of May 2017, there were only two
  # known datasets, the second occurring on all tested Android devices. If this
  # test fails on a new or different platform, examine whether the expected data
  # is drastically different. See crbug.com/723031 for more information.
  @Slow
  def testVideoAudio(self):
    alt_data = None
    is_android = ParseFlags().android
    if is_android:
      alt_data = 'data/buck_bunny_640x360_24fps.mp4.expected_volume_alt.json'
    self.instrumentedVideoTest('http://check.googlezip.net/cacheable/video/buck_bunny_640x360_24fps_audio.html',
      alt_data=alt_data)

  def instrumentedVideoTest(self, url, alt_data=None):
    """Run an instrumented video test. The given page is reloaded up to some
    maximum number of times until a compressed video is seen by ChromeDriver by
    inspecting the network logs. Once that happens, test.ready is set and that
    will signal the Javascript test on the page to begin. Once it is complete,
    check the results.
    """
    # The maximum number of times to attempt to reload the page for a compressed
    # video.
    max_attempts = 10
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--autoplay-policy=no-user-gesture-required')
      loaded_compressed_video = False
      attempts = 0
      while not loaded_compressed_video and attempts < max_attempts:
        t.LoadURL(url)
        attempts += 1
        for resp in t.GetHTTPResponses():
          if ('content-type' in resp.response_headers
              and resp.response_headers['content-type'] == 'video/webm'):
            loaded_compressed_video = True
            self.assertHasProxyHeaders(resp)
          else:
            # Take a breath before requesting again.
            time.sleep(1)
      if attempts >= max_attempts:
        self.fail('Could not get a compressed video after %d tries' % attempts)
      if alt_data != None:
        t.ExecuteJavascriptStatement('test.expectedVolumeSrc = "%s"' % alt_data)
      t.ExecuteJavascriptStatement('test.ready = true')
      t.WaitForJavascriptExpression('test.video_ != undefined', 5)
      # Click the video to start if Android.
      if ParseFlags().android:
        t.FindElement(By.ID, 'video').click()
      else:
        t.ExecuteJavascriptStatement('test.video_.play()')
      waitTimeQuery = 'test.waitTime'
      if ParseFlags().android:
        waitTimeQuery = 'test.androidWaitTime'
      wait_time = int(t.ExecuteJavascriptStatement(waitTimeQuery))
      t.WaitForJavascriptExpression('test.metrics.complete', wait_time)
      metrics = t.ExecuteJavascriptStatement('test.metrics')
      if not metrics['complete']:
        self.fail('Test not complete after %d seconds.' % wait_time)
      if metrics['failed']:
        self.fail('Test failed! ' + metrics['detailedStatus'])

  # Make sure YouTube autoplays.
  def testYoutube(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.LoadURL('http://data-saver-test.appspot.com/youtube')
      if ParseFlags().android:
        # Video won't auto play on Android, so give it a click.
        t.FindElement(By.ID, 'player').click()
      t.WaitForJavascriptExpression(
        'window.playerState == YT.PlayerState.PLAYING', 30)
      for response in t.GetHTTPResponses():
        if not response.url.startswith('https'):
          self.assertHasProxyHeaders(response)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
