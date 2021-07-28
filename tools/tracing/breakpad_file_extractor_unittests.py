#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import breakpad_file_extractor
import tempfile
import shutil

from unittest.mock import MagicMock, call


class ExtractBreakpadTestCase(unittest.TestCase):
  def setUp(self):
    # Create test inputs for ExtractBreakpadFiles() function.
    self.test_build_dir = tempfile.mkdtemp()
    self.test_breakpad_dir = tempfile.mkdtemp()
    self.test_dump_syms_dir = tempfile.mkdtemp()

    # NamedTemporaryFile() is hard coded to have a set of random 8 characters
    # appended to whatever prefix is given. Those characters can't be easily
    # removed, so |self.test_dump_syms_binary| is opened this way.
    self.test_dump_syms_binary = os.path.join(self.test_dump_syms_dir,
                                              'dump_syms')
    with open(self.test_dump_syms_binary, 'w'):
      pass

  def tearDown(self):
    shutil.rmtree(self.test_build_dir)
    shutil.rmtree(self.test_breakpad_dir)
    shutil.rmtree(self.test_dump_syms_dir)

  def _checkExtractWithOneBinary(self, dump_syms_path, build_dir, breakpad_dir):
    # Create test file in |test_build_dir| and test file in |test_breakpad_dir|.
    test_input_file = tempfile.NamedTemporaryFile(suffix='.so', dir=build_dir)
    # |test_output_file_path| requires a specific name, so NamedTemporaryFile()
    # is not used.
    intput_file_name = os.path.split(test_input_file.name)[1]
    test_output_file_path = '{output_path}.breakpad'.format(
        output_path=os.path.join(breakpad_dir, intput_file_name))
    with open(test_output_file_path, 'w'):
      pass

    # Create tempfiles that should be ignored.
    temp_toc_file = tempfile.NamedTemporaryFile(suffix='.TOC', dir=build_dir)
    temp_java_file = tempfile.NamedTemporaryFile(suffix='.java', dir=build_dir)
    temp_zip_file = tempfile.NamedTemporaryFile(suffix='.zip', dir=build_dir)
    temp_apk_file = tempfile.NamedTemporaryFile(suffix='_apk', dir=build_dir)

    breakpad_file_extractor._RunDumpSyms = MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles(dump_syms_path, build_dir,
                                                 breakpad_dir)

    breakpad_file_extractor._RunDumpSyms.assert_called_once_with(
        dump_syms_path, test_input_file.name, test_output_file_path)

    # Check that one file exists in the output directory.
    self.assertEqual(len(os.listdir(breakpad_dir)), 1)
    self.assertEqual(
        os.listdir(breakpad_dir)[0],
        os.path.basename(test_input_file.name) + '.breakpad')

  def testOneBinaryFile(self):
    self._checkExtractWithOneBinary(self.test_dump_syms_binary,
                                    self.test_build_dir, self.test_breakpad_dir)

  def testDumpSymsInBuildDir(self):
    new_dump_syms_path = os.path.join(self.test_build_dir, 'dump_syms')
    with open(new_dump_syms_path, 'w'):
      pass
    self._checkExtractWithOneBinary(new_dump_syms_path, self.test_build_dir,
                                    self.test_breakpad_dir)

  def testSymbolsInLibUnstrippedFolder(self):
    os.path.join(self.test_build_dir, 'lib.unstripped')
    self._checkExtractWithOneBinary(self.test_dump_syms_binary,
                                    self.test_build_dir, self.test_breakpad_dir)

  def testMultipleBinaryFiles(self):
    # Create files in |test_build_dir|.
    input_files = []
    suffixes = ['.so', '.exe', '_chrome']
    for suffix in suffixes:
      file = tempfile.NamedTemporaryFile(suffix=suffix, dir=self.test_build_dir)
      input_files.append(file)

    output_file_paths = []
    for file in input_files:
      input_file_name = os.path.split(file.name)[1]
      test_output_file_path = '{output_path}.breakpad'.format(
          output_path=os.path.join(self.test_breakpad_dir, input_file_name))
      with open(test_output_file_path, 'w'):
        pass
      output_file_paths.append(test_output_file_path)

    breakpad_file_extractor._RunDumpSyms = MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                 self.test_build_dir,
                                                 self.test_breakpad_dir)

    # Check that each call expected call to _RunDumpSyms() has been made.
    expected_calls = [
        call(self.test_dump_syms_binary, input_files[0].name,
             output_file_paths[0]),
        call(self.test_dump_syms_binary, input_files[1].name,
             output_file_paths[1]),
        call(self.test_dump_syms_binary, input_files[2].name,
             output_file_paths[2])
    ]
    breakpad_file_extractor._RunDumpSyms.assert_has_calls(expected_calls,
                                                          any_order=True)

    # Check that the expected files exist in the output directory.
    expected_files = [
        os.path.basename(f.name) + '.breakpad' for f in input_files
    ]
    file_count = 0
    for file in os.listdir(self.test_breakpad_dir):
      file_path = os.path.join(self.test_breakpad_dir, file)
      if os.path.isfile(file_path):
        file_count += 1
    self.assertEqual(file_count, 3)
    self.assertEqual(set(expected_files),
                     set(os.listdir(self.test_breakpad_dir)))

  def testDumpSymsNotFound(self):
    breakpad_file_extractor._RunDumpSyms = MagicMock()
    with self.assertRaisesRegex(FileNotFoundError, 'dump_syms is missing.'):
      breakpad_file_extractor.ExtractBreakpadFiles('fake/path/dump_syms',
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)

  def testFakeDirectories(self):
    breakpad_file_extractor._RunDumpSyms = MagicMock()
    with self.assertRaisesRegex(FileNotFoundError,
                                'Invalid breakpad output directory'):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   'fake_breakpad_dir')
    with self.assertRaisesRegex(FileNotFoundError, 'Invalid build directory'):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   'fake_binary_dir',
                                                   self.test_breakpad_dir)

  def testSymbolizedNoFiles(self):
    with self.assertRaisesRegex(
        Exception, 'Could not create breakpad symbols from any files'):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)

  def testIgnoredwp(self):
    dwp_file1 = os.path.join(self.test_build_dir, 'name.so.dwp')
    dwp_file2 = os.path.join(self.test_build_dir, 'name.so.dwo')
    with open(dwp_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(dwp_file2, 'w') as file2:
      file2.write('MODULE Linux x86_64 34984AB4EF948C name2.so')

    exception_msg = ('Could not create breakpad symbols from any files from ' +
                     self.test_build_dir + '.')
    with self.assertRaisesRegex(Exception, exception_msg):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)

    os.remove(dwp_file1)
    os.remove(dwp_file2)

  def testIgnorePartitionFiles(self):
    partition_file = os.path.join(self.test_build_dir, 'partition.so')
    with open(partition_file, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')

    exception_msg = ('Could not create breakpad symbols from any files from ' +
                     self.test_build_dir + '.')
    with self.assertRaisesRegex(Exception, exception_msg):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)

    os.remove(partition_file)

  def testIgnoreCombinedFiles(self):
    combined_file1 = os.path.join(self.test_build_dir, 'chrome_combined.so')
    combined_file2 = os.path.join(self.test_build_dir, 'libchrome_combined.so')
    with open(combined_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(combined_file2, 'w') as file2:
      file2.write('MODULE Linux x86_64 34984AB4EF948C name2.so')

    exception_msg = ('Could not create breakpad symbols from any files from ' +
                     self.test_build_dir + '.')
    with self.assertRaisesRegex(Exception, exception_msg):
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)

    os.remove(combined_file1)
    os.remove(combined_file2)


if __name__ == "__main__":
  unittest.main()
