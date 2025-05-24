# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import presubmit_caching_support


class DummyResult:

  def __init__(self, msg: str):
    self.msg = msg


def _TempCacheFile():
  file_handle, file_path = tempfile.mkstemp(suffix='.json', text=True)
  os.close(file_handle)
  return file_path


class PresubmitCachingSupportTest(unittest.TestCase):

  def testCanCacheAndRetrieveResult(self):
    storage_path = _TempCacheFile()
    cache = presubmit_caching_support.PresubmitCache(storage_path,
                                                     os.path.dirname(__file__))
    cache.StoreResultInCache(1, DummyResult('dummy result'))

    retrieved_result = cache.RetrieveResultFromCache(1)
    self.assertEqual(retrieved_result.msg, 'dummy result')

  def testCanReloadCacheFromDisk(self):
    storage_path = _TempCacheFile()

    cache = presubmit_caching_support.PresubmitCache(storage_path,
                                                     os.path.dirname(__file__))
    cache.StoreResultInCache(1, DummyResult('dummy result'))

    restored_cache = presubmit_caching_support.PresubmitCache(
        storage_path, os.path.dirname(__file__))
    retrieved_result = restored_cache.RetrieveResultFromCache(1)
    self.assertIsNotNone(retrieved_result)
    self.assertEqual(retrieved_result.msg, 'dummy result')

  def testDoesntReturnFromDifferentCheck(self):
    storage_path = _TempCacheFile()

    cache = presubmit_caching_support.PresubmitCache(storage_path,
                                                     os.path.dirname(__file__))
    cache.StoreResultInCache(1, DummyResult('dummy result'))

    restored_cache = presubmit_caching_support.PresubmitCache(
        storage_path, os.path.dirname(__file__))
    retrieved_result = restored_cache.RetrieveResultFromCache(17)
    self.assertIsNone(retrieved_result)

  def testChangingDirectoryContentInvalidatesCache(self):
    checked_dir = tempfile.mkdtemp()
    storage_path = _TempCacheFile()
    cache = presubmit_caching_support.PresubmitCache(storage_path, checked_dir)
    cache.StoreResultInCache(1, DummyResult('dummy result'))

    retrieved_result = cache.RetrieveResultFromCache(1)
    self.assertEqual(retrieved_result.msg, 'dummy result')

    with open(os.path.join(checked_dir, 'dummy_file'), 'w') as f:
      f.write('changing contents of observed directory')

    # The cache should be invalidated by the change in the observed directory.
    new_retrieved_result = cache.RetrieveResultFromCache(1)
    self.assertIsNone(new_retrieved_result)


if __name__ == '__main__':
  unittest.main()
