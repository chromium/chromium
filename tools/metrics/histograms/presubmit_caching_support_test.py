# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import setup_modules

import chromium_src.tools.metrics.histograms.presubmit_caching_support as presubmit_caching_support


class DummyResult:

  def __init__(self, msg: str):
    self.msg = msg


def _TempCacheFile():
  file_handle, file_path = tempfile.mkstemp(suffix='.json', text=True)
  os.close(file_handle)
  return file_path

class PresubmitCachingSupportTest(unittest.TestCase):

  def setUp(self):
    self.checked_dir = tempfile.mkdtemp()
    self.storage_path = _TempCacheFile()
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

  checked_dir: str = ""
  storage_path: str = ""
  cache: presubmit_caching_support.PresubmitCache = None


if __name__ == '__main__':
  unittest.main()
