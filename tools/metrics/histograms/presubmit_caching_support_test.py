# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

from typing import Optional

import setup_modules

import chromium_src.tools.metrics.histograms.presubmit_caching_support as presubmit_caching_support


class DummyResult:

  def __init__(self, msg: str):
    self.msg = msg


def _TempCacheDir():
  return tempfile.mkdtemp()


def _prepend_text_to_file(file_path: str, new_text: str):
  extra_bytes = bytes(new_text, encoding="utf-8")

  with open(file_path, 'rb') as file:
    original_content = file.read()

  with open(file_path, 'wb') as file:
    file.write(extra_bytes)
    file.write(original_content)

class PresubmitCachingSupportTest(unittest.TestCase):
  checked_dir: str = ""
  storage_path: str = ""
  cache: Optional[presubmit_caching_support.PresubmitCache] = None

  def setUp(self):
    self.checked_dir = tempfile.mkdtemp()
    self.storage_path = _TempCacheDir()
    self.cache = presubmit_caching_support.PresubmitCache(
        self.storage_path, self.checked_dir)
    self.cache.StoreResultInCache(1, DummyResult('dummy result'))

  def testCanCacheAndRetrieveResult(self):
    retrieved_result = self.cache.RetrieveResultFromCache(1)
    self.assertEqual(retrieved_result.msg, 'dummy result')

  def testCanReloadCacheFromDisk(self):
    restored_cache = presubmit_caching_support.PresubmitCache(
        self.storage_path, self.checked_dir)
    retrieved_result = restored_cache.RetrieveResultFromCache(1)
    self.assertIsNotNone(retrieved_result)
    self.assertEqual(retrieved_result.msg, 'dummy result')

  def testDoesntReturnFromDifferentCheck(self):
    restored_cache = presubmit_caching_support.PresubmitCache(
        self.storage_path, self.checked_dir)
    retrieved_result = restored_cache.RetrieveResultFromCache(17)
    self.assertIsNone(retrieved_result)

  def testChangingDirectoryContentInvalidatesCache(self):
    retrieved_result = self.cache.RetrieveResultFromCache(1)
    self.assertEqual(retrieved_result.msg, 'dummy result')

    with open(os.path.join(self.checked_dir, 'dummy_file'), 'w') as f:
      f.write('changing contents of observed directory')

    # The cache should be invalidated by the change in the observed directory.
    new_retrieved_result = self.cache.RetrieveResultFromCache(1)
    self.assertIsNone(new_retrieved_result)

  def testBrokenCleansTheCache(self):
    cache_storage_file = self.cache._storage_file_path
    _prepend_text_to_file(cache_storage_file, "THIS_IS_INVALID_PICKLE")

    restored_cache = presubmit_caching_support.PresubmitCache(
        self.storage_path, self.checked_dir)

    self.assertIsNone(restored_cache.RetrieveResultFromCache(1))
    self.assertFalse(os.path.exists(self.cache._storage_file_path))

if __name__ == '__main__':
  unittest.main()
