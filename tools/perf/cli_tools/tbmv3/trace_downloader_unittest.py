# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
from unittest import mock

from cli_tools.tbmv3 import trace_downloader
from py_utils import cloud_storage


class TraceDownloaderTests(unittest.TestCase):
  def setUp(self):
    self._cs_exists = mock.patch(
        'cli_tools.tbmv3.trace_downloader.cloud_storage.Exists').start()
    self._cs_list = mock.patch(
        'cli_tools.tbmv3.trace_downloader.cloud_storage.List').start()
    self._cs_get = mock.patch(
        'cli_tools.tbmv3.trace_downloader.cloud_storage.Get').start()

  def tearDown(self):
    mock.patch.stopall()

  def testGetLocalTraceFileName(self):
    computed_name = trace_downloader.GetLocalTraceFileName(
        'https://storage.cloud.google.com/chrome-telemetry-output/'
        '20201029T003106_99943/v8.browsing_mobile/'
        'browse_shopping_amazon_2019/retry_0/trace.html')

    expected_name = ('20201029T003106_99943_v8.browsing_mobile_'
                     'browse_shopping_amazon_2019_retry_0_trace')
    self.assertEqual(computed_name, expected_name)

  def testGetFileExtension(self):
    filepath = 'foo.bar/a/b.pb.gz.html'
    self.assertEqual(trace_downloader.GetFileExtension(filepath), '.pb.gz.html')
    self.assertEqual(trace_downloader.GetFileExtension('a/b/c'), '')

  def testFindProtoTracePathSimple(self):
    url = trace_downloader.HTML_URL_PREFIX + 'foo/bar/trace.html'
    self._cs_list.side_effect = Exception

    self._cs_exists.side_effect = lambda x, y: y == 'foo/bar/trace.pb'
    proto_path = trace_downloader.FindProtoTracePath(url)
    self.assertEqual(proto_path, 'foo/bar/trace.pb')

    self._cs_exists.side_effect = lambda x, y: y == 'foo/bar/trace.pb.gz'
    proto_path = trace_downloader.FindProtoTracePath(url)
    self.assertEqual(proto_path, 'foo/bar/trace.pb.gz')

  def testFindProtoTracePathComplex(self):
    url = trace_downloader.HTML_URL_PREFIX + 'foo/bar/trace.html'
    self._cs_exists.return_value = False

    self._cs_list.side_effect = lambda x, y: []
    with self.assertRaises(cloud_storage.NotFoundError):
      proto_path = trace_downloader.FindProtoTracePath(url)

    ret_files = ['/foo/bar/trace/traceEvents/tmp1234.pb']
    self._cs_list.side_effect = lambda x, y: ret_files
    proto_path = trace_downloader.FindProtoTracePath(url)
    self.assertEqual(proto_path, 'foo/bar/trace/traceEvents/tmp1234.pb')

    ret_files = [
        '/foo/bar/trace/traceEvents/123abc.pb.gz',
        '/foo/bar/trace/traceEvents/tmp1234.pb'
    ]
    self._cs_list.side_effect = lambda x, y: ret_files
    proto_path = trace_downloader.FindProtoTracePath(url)
    self.assertEqual(proto_path, 'foo/bar/trace/traceEvents/123abc.pb.gz')

    ret_files = ['/foo/bar/trace/traceEvents/123abc.foo']
    self._cs_list.side_effect = lambda x, y: ret_files
    with self.assertRaises(cloud_storage.NotFoundError):
      proto_path = trace_downloader.FindProtoTracePath(url)

  def testDownloadHtmlTrace(self):
    url = trace_downloader.HTML_URL_PREFIX + 'foo/bar/trace.html'
    trace_path = trace_downloader.DownloadHtmlTrace(url)
    self._cs_get.assert_called_once()
    self.assertEqual(
        trace_path,
        os.path.join(trace_downloader.DEFAULT_TRACE_DIR, 'foo_bar_trace.html'))

  def testDownloadProtoTrace(self):
    url = trace_downloader.HTML_URL_PREFIX + 'foo/bar/trace.html'
    self._cs_exists.side_effect = lambda x, y: y == 'foo/bar/trace.pb.gz'
    trace_path = trace_downloader.DownloadProtoTrace(url)
    self._cs_get.assert_called_once()
    self.assertEqual(
        trace_path,
        os.path.join(trace_downloader.DEFAULT_TRACE_DIR, 'foo_bar_trace.pb.gz'))
