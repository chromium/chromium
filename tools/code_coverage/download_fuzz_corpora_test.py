# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from unittest.mock import patch

import download_fuzz_corpora


class DownloadFuzzCorporaTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.fake_binary_dir = tempfile.mkdtemp()
    for binary in [
      "fake_1_fuzzer",
      "fake_2_fuzzer",
      "fake_win_fuzzer.exe",
      "some_other_binary",
    ]:
      f = open(os.path.join(cls.fake_binary_dir, binary), "x")
      f.write(binary)
      f.close()
    cls.fake_download_dir = tempfile.mkdtemp()
    cls.chromium_src_dir = os.path.join(
      os.path.abspath(os.path.dirname(__file__)), "..", ".."
    )

  @classmethod
  def tearDownClass(cls):
    shutil.rmtree(cls.fake_binary_dir, ignore_errors=True)
    shutil.rmtree(cls.fake_download_dir, ignore_errors=True)

  def test_wrong_arguments(self):
    cmd = [
      sys.executable,
      'tools/code_coverage/download_fuzz_corpora.py',
      '--download-dir',
      self.__class__.fake_download_dir,
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
      assert "returned non-zero exit status 2" in str(e.exception)
    cmd = [
      sys.executable,
      'tools/code_coverage/download_fuzz_corpora.py',
      '--build-dir',
      self.__class__.fake_binary_dir,
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
      assert "returned non-zero exit status 2" in str(e.exception)

  def test_download_fuzz_corpora(self):

    def mock_gsutil(cmd, cwd):
      target_dir = os.path.join(cwd, cmd[2])
      os.makedirs(target_dir, exist_ok=True)
      zip_path = os.path.join(target_dir, 'latest.zip')
      with zipfile.ZipFile(zip_path, 'w') as zf:
        zf.writestr('test.txt', 'test')

    with patch(
      'download_fuzz_corpora._ParseCommandArguments'
    ) as _ParseCommandArgumentsMock:
      with patch('download_fuzz_corpora._gsutil', side_effect=mock_gsutil):
        _ParseCommandArgumentsMock.return_value = argparse.Namespace(
          download_dir=self.__class__.fake_download_dir,
          build_dir=self.__class__.fake_binary_dir,
          corpora_type='libfuzzer',
          arch='x64',
        )
        download_fuzz_corpora.Main()
        self.assertTrue(
          os.path.isdir(
            os.path.join(self.__class__.fake_download_dir, "fake_1_fuzzer")
          )
        )
        self.assertTrue(
          os.path.isdir(
            os.path.join(self.__class__.fake_download_dir, "fake_2_fuzzer")
          )
        )
        self.assertTrue(
          os.path.isdir(
            os.path.join(self.__class__.fake_download_dir, "fake_win_fuzzer")
          )
        )
        self.assertFalse(
          os.path.isdir(
            os.path.join(self.__class__.fake_binary_dir, "some_other_binary")
          )
        )

  def test_unzip_corpus(self):
    target_dir = os.path.join(self.__class__.fake_download_dir, "test_target")
    os.makedirs(target_dir, exist_ok=True)
    zip_path = os.path.join(target_dir, "latest.zip")
    with zipfile.ZipFile(zip_path, "w") as zf:
      zf.writestr("test_file.txt", "content")
      zf.writestr("regressions/reg1.txt", "regression content")

    download_fuzz_corpora._unzip_corpus(
      ("test_target", self.__class__.fake_download_dir)
    )
    self.assertTrue(os.path.exists(os.path.join(target_dir, "test_file.txt")))
    self.assertFalse(os.path.exists(zip_path))
    self.assertFalse(os.path.exists(os.path.join(target_dir, "regressions")))

  def test_unzip_fuzzilli_corpus(self):
    target_dir = os.path.join(self.__class__.fake_download_dir, "autozilli-1")
    os.makedirs(target_dir, exist_ok=True)
    tgz_path = os.path.join(target_dir, "autozilli-1.tgz")
    sample_file = os.path.join(self.__class__.fake_download_dir, "sample.js")
    with open(sample_file, "w") as f:
      f.write("print(1);")
    with tarfile.open(tgz_path, "w:gz") as tf:
      tf.add(sample_file, arcname="sample.js")
    os.remove(sample_file)

    download_fuzz_corpora._unzip_fuzzilli_corpus(
      ("autozilli-1.tgz", self.__class__.fake_download_dir)
    )
    self.assertTrue(os.path.exists(os.path.join(target_dir, "sample.js")))
    self.assertFalse(os.path.exists(tgz_path))


if __name__ == '__main__':
  unittest.main()
