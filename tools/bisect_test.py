# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import subprocess

bisect_builds = __import__('bisect-builds')


class FakeProcess:
  called_num_times = 0

  def __init__(self, returncode):
    self.returncode = returncode
    FakeProcess.called_num_times += 1

  def communicate(self):
    return ('', '')


class BisectTest(unittest.TestCase):

  patched = []
  max_rev = 10000
  fake_process_return_code = 0

  def monkey_patch(self, obj, name, new):
    self.patched.append((obj, name, getattr(obj, name)))
    setattr(obj, name, new)

  def clear_patching(self):
    for obj, name, old in self.patched:
      setattr(obj, name, old)
    self.patched = []

  def setUp(self):
    FakeProcess.called_num_times = 0
    self.fake_process_return_code = 0
    self.monkey_patch(bisect_builds.DownloadJob, 'Start', lambda *args: None)
    self.monkey_patch(bisect_builds.DownloadJob, 'Stop', lambda *args: None)
    self.monkey_patch(bisect_builds.DownloadJob, 'WaitFor', lambda *args: None)
    self.monkey_patch(bisect_builds, 'UnzipFilenameToDir', lambda *args: None)
    self.monkey_patch(
        subprocess, 'Popen',
        lambda *args, **kwargs: FakeProcess(self.fake_process_return_code))
    self.monkey_patch(bisect_builds.PathContext, 'ParseDirectoryIndex',
                      lambda *args: range(self.max_rev))

  def tearDown(self):
    self.clear_patching()

  def bisect(self, good_rev, bad_rev, evaluate, num_runs=1):
    base_url = bisect_builds.CHROMIUM_BASE_URL
    archive = 'linux'
    asan = False
    use_local_cache = False
    context = bisect_builds.PathContext(base_url, archive, good_rev, bad_rev,
                                        asan, use_local_cache)
    (minrev, maxrev, _) = bisect_builds.Bisect(
        context=context,
        evaluate=evaluate,
        num_runs=num_runs,
        profile=None,
        try_args=[])
    return (minrev, maxrev)

  def testBisectConsistentAnswer(self):
    self.assertEqual(self.bisect(1000, 100, lambda *args: 'g'), (100, 101))
    self.assertEqual(self.bisect(100, 1000, lambda *args: 'b'), (100, 101))
    self.assertEqual(self.bisect(2000, 200, lambda *args: 'b'), (1999, 2000))
    self.assertEqual(self.bisect(200, 2000, lambda *args: 'g'), (1999, 2000))

  def testBisectMultipleRunsEarlyReturn(self):
    self.fake_process_return_code = 1
    self.assertEqual(self.bisect(1, 3, lambda *args: 'b', num_runs=10), (1, 2))
    self.assertEqual(FakeProcess.called_num_times, 1)

  def testBisectAllRunsWhenAllSucceed(self):
    self.assertEqual(self.bisect(1, 3, lambda *args: 'b', num_runs=10), (1, 2))
    self.assertEqual(FakeProcess.called_num_times, 10)


if __name__ == '__main__':
  unittest.main()
