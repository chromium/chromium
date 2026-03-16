# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest
import sys


class RunAllFuzzersTest(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    if sys.platform != 'linux':
      return
    gn_args = ('use_clang_coverage=true '
               'dcheck_always_on=true '
               'ffmpeg_branding=\"ChromeOS\" '
               'is_component_build=false '
               'is_debug=false '
               'proprietary_codecs=true '
               'use_remoteexec=true '
               'use_libfuzzer=true '
               'use_clang_modules=false')
    cls.testfuzzer1 = 'xml_parser_fuzzer'
    cls.testfuzzer2 = 'query_parser_fuzzer'
    cls.chromium_src_dir = os.path.join(
        os.path.abspath(os.path.dirname(__file__)), "..", "..")
    fuzzer_binaries_dir = "out/run_all_fuzzers_test"
    pathlib.Path(fuzzer_binaries_dir).mkdir(parents=True, exist_ok=True)
    cls.fuzzer_binaries_dir = fuzzer_binaries_dir
    cls.fuzzer_corpora_dir = tempfile.mkdtemp()
    cls.profdata_outdir = tempfile.mkdtemp()
    os.mkdir(os.path.join(cls.fuzzer_corpora_dir, cls.testfuzzer1))
    corporadir1 = os.path.join(cls.fuzzer_corpora_dir, cls.testfuzzer1)
    os.mkdir(os.path.join(cls.fuzzer_corpora_dir, cls.testfuzzer2))
    corporadir2 = os.path.join(cls.fuzzer_corpora_dir, cls.testfuzzer2)
    for letter in ["a", "b", "c", "d"]:
      f1 = open(os.path.join(corporadir1, letter), "x")
      f2 = open(os.path.join(corporadir2, letter), "x")
      f1.write(letter)
      f2.write(letter)
      f1.close()
      f2.close()
    gn_gen_cmd = ['gn', 'gen', cls.fuzzer_binaries_dir, '--args=%s' % gn_args]
    try:
      subprocess.check_output(gn_gen_cmd, cwd=cls.chromium_src_dir)
    except subprocess.CalledProcessError as e:
      print("GN command failed. Error:")
      print(e.output)
    build_cmd = [
        'autoninja', '-C', cls.fuzzer_binaries_dir, cls.testfuzzer1,
        cls.testfuzzer2
    ]
    with open("test.log", "wb") as f:
      try:
        process = subprocess.Popen(build_cmd,
                                   cwd=cls.chromium_src_dir,
                                   stdout=subprocess.PIPE)
        for c in iter(lambda: process.stdout.read(1), b''):
          f.write(c)
      except subprocess.CalledProcessError as e:
        print("Build command failed. Error:")
        print(e.output)

  @classmethod
  def tearDownClass(cls):
    if sys.platform != 'linux':
      return
    # ignore_errors allows us to delete the directory even though the directory
    # is non-empty. This is what we want, since we created these temporarily,
    # only for the purpose of tests.
    shutil.rmtree(cls.fuzzer_binaries_dir, ignore_errors=True)
    shutil.rmtree(cls.fuzzer_corpora_dir, ignore_errors=True)
    shutil.rmtree(cls.profdata_outdir, ignore_errors=True)

  def test_wrong_arguments(self):
    if sys.platform != 'linux':
      return
    cmd = [
        'python3', 'tools/code_coverage/run_all_fuzzers.py',
        '--fuzzer-binaries-dir', self.__class__.fuzzer_binaries_dir,
        '--fuzzer-corpora-dir', self.__class__.fuzzer_corpora_dir
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
    assert ("returned non-zero exit status 2" in str(e.exception))
    cmd = [
        'python3', 'tools/code_coverage/run_all_fuzzers.py',
        '--fuzzer-binaries-dir', self.__class__.fuzzer_binaries_dir,
        '--profdata-outdir', self.__class__.profdata_outdir
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
    assert ("returned non-zero exit status 2" in str(e.exception))
    cmd = [
        'python3', 'tools/code_coverage/run_all_fuzzers.py',
        '--fuzzer-corpora-dir', self.__class__.fuzzer_corpora_dir,
        '--profdata-outdir', self.__class__.profdata_outdir
    ]
    with self.assertRaises(subprocess.CalledProcessError) as e:
      subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)
    assert ("returned non-zero exit status 2" in str(e.exception))

  def test_libfuzzer_fuzzers_succeed(self):
    if sys.platform != 'linux':
      return
    cmd = [
        'python3', 'tools/code_coverage/run_all_fuzzers.py',
        '--fuzzer-binaries-dir', self.__class__.fuzzer_binaries_dir,
        '--fuzzer-corpora-dir', self.__class__.fuzzer_corpora_dir,
        '--profdata-outdir', self.__class__.profdata_outdir, '--fuzzer',
        'libfuzzer'
    ]
    subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)

    expected_profdata = sorted([
        self.__class__.testfuzzer1 + ".profdata",
        self.__class__.testfuzzer2 + ".profdata"
    ])
    actual_profdata = sorted(os.listdir(self.__class__.profdata_outdir))
    assert (
        expected_profdata == actual_profdata
    ), "Expected " + str(expected_profdata) + " but got " + str(actual_profdata)

  def test_blackbox_fuzzers_succeed(self):
    if sys.platform != 'linux':
      return
    # Create a dummy chrome binary in a temp dir.
    with tempfile.TemporaryDirectory() as bin_dir:
      chrome_bin = os.path.join(bin_dir, "chrome")
      with open(chrome_bin, "w") as f:
        f.write("#!/bin/sh\n")
        f.write("exit 0\n")
      os.chmod(chrome_bin, 0o755)

      # Create dummy testcases in the corpora_dir directory.
      with tempfile.TemporaryDirectory() as corpora_dir:
        with open(os.path.join(corpora_dir, "test.html"), "w") as f:
          f.write("<html></html>")

        with tempfile.TemporaryDirectory() as out_dir:
          cmd = [
              sys.executable,
              'tools/code_coverage/run_all_fuzzers.py',
              '--fuzzer-binaries-dir',
              bin_dir,
              '--fuzzer-corpora-dir',
              corpora_dir,
              '--profdata-outdir',
              out_dir,
              '--fuzzer',
              'blackbox',
              '--target',
              'chrome',
          ]
          # Verify the script runs without crashing when blackbox is specified.
          subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)


if __name__ == '__main__':
  unittest.main()
