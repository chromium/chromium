# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import shutil
import tempfile
import unittest
import zlib

import mock

from core.services import isolate_service


def ContentResponse(content):
  return [{'content': base64.b64encode(zlib.compress(content))}]


def UrlResponse(url, content):
  return [{'url': url}, zlib.compress(content)]


class TestIsolateApi(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    mock.patch('core.services.isolate_service.CACHE_DIR', os.path.join(
        self.temp_dir, 'isolate_cache')).start()
    self.mock_request = mock.patch('core.services.request.Request').start()

  def tearDown(self):
    shutil.rmtree(self.temp_dir)
    mock.patch.stopall()

  def testRetrieve_content(self):
    self.mock_request.side_effect = ContentResponse('OK!')
    self.assertEqual(isolate_service.Retrieve('hash'), 'OK!')

  def testRetrieve_fromUrl(self):
    self.mock_request.side_effect = UrlResponse('http://get/response', 'OK!')
    self.assertEqual(isolate_service.Retrieve('hash'), 'OK!')

  def testRetrieveCompressed_content(self):
    self.mock_request.side_effect = ContentResponse('OK!')
    self.assertEqual(
        isolate_service.RetrieveCompressed('hash'), zlib.compress('OK!'))

  def testRetrieveCompressed_fromUrl(self):
    self.mock_request.side_effect = UrlResponse('http://get/response', 'OK!')
    self.assertEqual(
        isolate_service.RetrieveCompressed('hash'), zlib.compress('OK!'))

  def testRetrieveCompressed_usesCache(self):
    self.mock_request.side_effect = ContentResponse('OK!')
    self.assertEqual(
        isolate_service.RetrieveCompressed('hash'), zlib.compress('OK!'))
    self.assertEqual(
        isolate_service.RetrieveCompressed('hash'), zlib.compress('OK!'))
    # We retrieve the same hash twice, but the request is only made once.
    self.assertEqual(self.mock_request.call_count, 1)

  def testRetrieveFile_succeeds(self):
    self.mock_request.side_effect = (
        ContentResponse(json.dumps({'files': {'foo': {'h': 'hash2'}}})) +
        UrlResponse('http://get/file/contents', 'nice!'))

    self.assertEqual(isolate_service.RetrieveFile('hash1', 'foo'), 'nice!')

  def testRetrieveFile_fails(self):
    self.mock_request.side_effect = (
        ContentResponse(json.dumps({'files': {'foo': {'h': 'hash2'}}})) +
        UrlResponse('http://get/file/contents', 'nice!'))

    with self.assertRaises(KeyError):
      isolate_service.RetrieveFile('hash1', 'bar')  # File not in isolate.
