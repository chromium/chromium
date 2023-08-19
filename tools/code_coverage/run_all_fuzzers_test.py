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
               'is_component_build=true '
               'is_debug=false '
               'proprietary_codecs=true '
               'use_reclient=true '
               'use_libfuzzer=true')
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

  def test_all_fuzzers_succeed(self):
    if sys.platform != 'linux':
      return
    cmd = [
        'python3', 'tools/code_coverage/run_all_fuzzers.py',
        '--fuzzer-binaries-dir', self.__class__.fuzzer_binaries_dir,
        '--fuzzer-corpora-dir', self.__class__.fuzzer_corpora_dir,
        '--profdata-outdir', self.__class__.profdata_outdir
    ]
    subprocess.check_call(cmd, cwd=self.__class__.chromium_src_dir)

    expected_profdata = sorted([
        self.__class__.testfuzzer1 + ".profraw",
        self.__class__.testfuzzer2 + ".profraw"
    ])
    actual_profdata = sorted(os.listdir(self.__class__.profdata_outdir))
    assert (
        expected_profdata == actual_profdata
    ), "Expected " + str(expected_profdata) + " but got " + str(actual_profdata)


if __name__ == '__main__':
  unittest.main()
