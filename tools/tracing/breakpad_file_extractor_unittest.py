#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from logging import exception
import os
import sys
import unittest

import breakpad_file_extractor
import tempfile
import shutil

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

import metadata_extractor
from metadata_extractor import OSName
from core.tbmv3 import trace_processor

import mock


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
    input_file_name = os.path.split(test_input_file.name)[1]
    test_output_file_path = '{output_path}.breakpad'.format(
        output_path=os.path.join(breakpad_dir, input_file_name))
    with open(test_output_file_path, 'w'):
      pass

    # Create tempfiles that should be ignored when extracting symbol files.
    with tempfile.NamedTemporaryFile(
        suffix='.TOC', dir=build_dir), tempfile.NamedTemporaryFile(
            suffix='.java', dir=build_dir), tempfile.NamedTemporaryFile(
                suffix='.zip', dir=build_dir), tempfile.NamedTemporaryFile(
                    suffix='_apk', dir=build_dir), tempfile.NamedTemporaryFile(
                        suffix='.so.dwp',
                        dir=build_dir), tempfile.NamedTemporaryFile(
                            suffix='.so.dwo',
                            dir=build_dir), tempfile.NamedTemporaryFile(
                                suffix='_chromesymbols.zip', dir=build_dir):
      breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
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
    # Create files in |test_build_dir|. All files are removed when
    # |test_build_dir| is recursively deleted.
    input_filenames = []
    so_file = os.path.join(self.test_build_dir, 'test_file.so')
    with open(so_file, 'w') as _:
      pass
    input_filenames.append(so_file)
    exe_file = os.path.join(self.test_build_dir, 'test_file.exe')
    with open(exe_file, 'w') as _:
      pass
    input_filenames.append(exe_file)
    chrome_file = os.path.join(self.test_build_dir, 'chrome')
    with open(chrome_file, 'w') as _:
      pass
    input_filenames.append(chrome_file)

    # Form output file paths.
    output_file_paths = []
    for file_iter in input_filenames:
      input_file_name = os.path.split(file_iter)[1]
      test_output_file_path = '{output_path}.breakpad'.format(
          output_path=os.path.join(self.test_breakpad_dir, input_file_name))
      with open(test_output_file_path, 'w'):
        pass
      output_file_paths.append(test_output_file_path)
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                 self.test_build_dir,
                                                 self.test_breakpad_dir)

    # Check that each call expected call to _RunDumpSyms() has been made.
    expected_calls = [
        mock.call(self.test_dump_syms_binary, input_filename,
                  output_file_paths[file_iter])
        for file_iter, input_filename in enumerate(input_filenames)
    ]
    breakpad_file_extractor._RunDumpSyms.assert_has_calls(expected_calls,
                                                          any_order=True)

    # Check that the expected files exist in the output directory.
    expected_files = [
        os.path.basename(f) + '.breakpad' for f in input_filenames
    ]
    file_count = 0
    for filename in os.listdir(self.test_breakpad_dir):
      file_path = os.path.join(self.test_breakpad_dir, filename)
      if os.path.isfile(file_path):
        file_count += 1
    self.assertEqual(file_count, 3)
    self.assertEqual(set(expected_files),
                     set(os.listdir(self.test_breakpad_dir)))

  def testDumpSymsNotFound(self):
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
    exception_msg = 'dump_syms is missing.'
    with self.assertRaises(Exception) as e:
      breakpad_file_extractor.ExtractBreakpadFiles('fake/path/dump_syms',
                                                   self.test_build_dir,
                                                   self.test_breakpad_dir)
    self.assertIn(exception_msg, str(e.exception))

  def testFakeDirectories(self):
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
    exception_msg = 'Invalid breakpad output directory'
    with self.assertRaises(Exception) as e:
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   self.test_build_dir,
                                                   'fake_breakpad_dir')
    self.assertIn(exception_msg, str(e.exception))

    exception_msg = 'Invalid build directory'
    with self.assertRaises(Exception) as e:
      breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                   'fake_binary_dir',
                                                   self.test_breakpad_dir)
    self.assertIn(exception_msg, str(e.exception))

  def testSymbolizedNoFiles(self):
    did_extract = breakpad_file_extractor.ExtractBreakpadFiles(
        self.test_dump_syms_binary, self.test_build_dir, self.test_breakpad_dir)
    self.assertFalse(did_extract)

  def testNotSearchUnstripped(self):
    # Make 'lib.unstripped' directory and file. Our script should not run
    # dump_syms on this file.
    lib_unstripped = os.path.join(self.test_build_dir, 'lib.unstripped')
    os.mkdir(lib_unstripped)
    lib_unstripped_file = os.path.join(lib_unstripped, 'unstripped.so')
    with open(lib_unstripped_file, 'w') as _:
      pass

    # Make file to run dump_syms on in input directory.
    extracted_file_name = 'extracted.so'
    extracted_file = os.path.join(self.test_build_dir, extracted_file_name)
    with open(extracted_file, 'w') as _:
      pass

    breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                 self.test_build_dir,
                                                 self.test_breakpad_dir,
                                                 search_unstripped=False)

    # Check that _RunDumpSyms() only called for extracted file and not the
    # lib.unstripped files.
    extracted_output_path = '{output_path}.breakpad'.format(
        output_path=os.path.join(self.test_breakpad_dir, extracted_file_name))
    breakpad_file_extractor._RunDumpSyms.assert_called_once_with(
        self.test_dump_syms_binary, extracted_file, extracted_output_path)

  def testIgnorePartitionFiles(self):
    partition_file = os.path.join(self.test_build_dir, 'partition.so')
    with open(partition_file, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')

    did_extract = breakpad_file_extractor.ExtractBreakpadFiles(
        self.test_dump_syms_binary, self.test_build_dir, self.test_breakpad_dir)
    self.assertFalse(did_extract)

    os.remove(partition_file)

  def testIgnoreCombinedFiles(self):
    combined_file1 = os.path.join(self.test_build_dir, 'chrome_combined.so')
    combined_file2 = os.path.join(self.test_build_dir, 'libchrome_combined.so')
    with open(combined_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(combined_file2, 'w') as file2:
      file2.write('MODULE Linux x86_64 34984AB4EF948C name2.so')

    did_extract = breakpad_file_extractor.ExtractBreakpadFiles(
        self.test_dump_syms_binary, self.test_build_dir, self.test_breakpad_dir)
    self.assertFalse(did_extract)

    os.remove(combined_file1)
    os.remove(combined_file2)


if __name__ == "__main__":
  unittest.main()
