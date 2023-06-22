# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from unittest.mock import patch
import download_fuzz_corpora
import tempfile
import unittest
import sys
import os
import shutil
import argparse
import subprocess


class DownloadFuzzCorporaTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    if sys.platform != 'linux':
      return
    cls.fake_binary_dir = tempfile.mkdtemp()
    for binary in ["fake_1_fuzzer", "fake_2_fuzzer", "some_other_binary"]:
      f = open(os.path.join(cls.fake_binary_dir, binary), "x")
      f.write(binary)
      f.close()
    cls.fake_download_dir = tempfile.mkdtemp()
    cls.chromium_src_dir = os.path.join(
        os.path.abspath(os.path.dirname(__file__)), "..", "..")

  @classmethod
  def tearDownClass(cls):
    if sys.platform != 'linux':
      return
    shutil.rmtree(cls.fake_binary_dir, ignore_errors=True)

  def test_wrong_arguments(self):
    if sys.platform != 'linux':
      return
    cmd = [
        'python3', 'tools/code_coverage/download_fuzz_corpora.py',
        '--download-dir', self.__class__.fake_download_dir
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
      assert ("returned non-zero exit status 2" in str(e.exception))
    cmd = [
        'python3', 'tools/code_coverage/download_fuzz_corpora.py',
        '--build-dir', self.__class__.fake_binary_dir
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
      assert ("returned non-zero exit status 2" in str(e.exception))

  def test_download_fuzz_corpora(self):
    with patch('download_fuzz_corpora._ParseCommandArguments'
               ) as _ParseCommandArgumentsMock:
      with patch('download_fuzz_corpora._gsutil') as _gsutil_mock:
        with patch('download_fuzz_corpora.unzip_corpora') as unzip_corpora_mock:
          _gsutil_mock.return_value = None
          _ParseCommandArgumentsMock.return_value = argparse.Namespace(
              download_dir=self.__class__.fake_download_dir,
              build_dir=self.__class__.fake_binary_dir)
          unzip_corpora_mock.return_value = None
          download_fuzz_corpora.Main()
          self.assertTrue(
              os.path.isdir(
                  os.path.join(self.__class__.fake_download_dir,
                               "fake_1_fuzzer")))
          self.assertTrue(
              os.path.isdir(
                  os.path.join(self.__class__.fake_download_dir,
                               "fake_2_fuzzer")))
          self.assertFalse(
              os.path.isdir(
                  os.path.join(self.__class__.fake_binary_dir,
                               "some_other_binary")))


if __name__ == '__main__':
  unittest.main()
