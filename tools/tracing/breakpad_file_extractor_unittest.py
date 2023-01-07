#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from logging import exception
import os
import shutil
import sys
import tempfile
import unittest

import breakpad_file_extractor
import get_symbols_util

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util

path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

import metadata_extractor

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

    # Stash function.
    self.RunDumpSyms_stash = breakpad_file_extractor._RunDumpSyms

  def tearDown(self):
    shutil.rmtree(self.test_build_dir)
    shutil.rmtree(self.test_breakpad_dir)
    shutil.rmtree(self.test_dump_syms_dir)

    # Unstash function.
    breakpad_file_extractor._RunDumpSyms = self.RunDumpSyms_stash

  def _setupSubtreeFiles(self):
    # Create subtree directory structure. All files deleted when
    # |test_breakpad_dir| is recursively deleted.
    out = tempfile.mkdtemp(dir=self.test_breakpad_dir)
    release = tempfile.mkdtemp(dir=out)
    subdir = tempfile.mkdtemp(dir=release)
    unstripped_dir = os.path.join(release, 'lib.unstripped')
    os.mkdir(unstripped_dir)

    # Create symbol files.
    symbol_files = []
    symbol_files.append(os.path.join(subdir, 'subdir.so'))
    symbol_files.append(os.path.join(unstripped_dir, 'unstripped.so'))
    symbol_files.append(os.path.join(unstripped_dir, 'unstripped2.so'))

    for new_file in symbol_files:
      with open(new_file, 'w') as _:
        pass

    # Build side effect mapping.
    side_effect_map = {
        symbol_files[0]:
        'MODULE Android x86_64 34984AB4EF948C0000000000000000000 subdir.so',
        symbol_files[1]: 'MODULE Android x86_64 34984AB4EF948D unstripped.so',
        symbol_files[2]: 'MODULE Android x86_64 34984AB4EF949A unstripped2.so'
    }

    return symbol_files, side_effect_map

  def _getDumpSymsMockSideEffect(self, side_effect_map):
    def run_dumpsyms_side_effect(dump_syms_binary,
                                 input_file_path,
                                 output_file_path,
                                 only_module_header=False):
      self.assertEqual(self.test_dump_syms_binary, dump_syms_binary)
      if only_module_header:
        # Extract Module ID.
        with open(output_file_path, 'w') as f:
          # Write the correct module header into the output f
          f.write(side_effect_map[input_file_path])
      else:
        # Extract breakpads.
        with open(output_file_path, 'w'):
          pass
      return True

    return run_dumpsyms_side_effect

  def _getExpectedModuleExtractionCalls(self, symbol_files):
    expected_module_calls = [
        mock.call(self.test_dump_syms_binary,
                  symbol_fle,
                  mock.ANY,
                  only_module_header=True) for symbol_fle in symbol_files
    ]
    return expected_module_calls

  def _getExpectedBreakpadExtractionCalls(self, extracted_files,
                                          breakpad_files):
    expected_extract_calls = [
        mock.call(self.test_dump_syms_binary, extracted_file,
                  breakpad_files[file_iter])
        for file_iter, extracted_file in enumerate(extracted_files)
    ]
    return expected_extract_calls

  def _getAndEnsureExtractedBreakpadFiles(self, extracted_files):
    breakpad_files = []
    for extracted_file in extracted_files:
      breakpad_filename = os.path.basename(extracted_file) + '.breakpad'
      breakpad_file = os.path.join(self.test_breakpad_dir, breakpad_filename)
      assert (os.path.isfile(breakpad_file))
      breakpad_files.append(breakpad_file)
    return breakpad_files

  def _getAndEnsureExpectedSubtreeBreakpadFiles(self, extracted_files):
    breakpad_files = []
    for extracted_file in extracted_files:
      breakpad_file = extracted_file + '.breakpad'
      assert (os.path.isfile(breakpad_file))
      breakpad_files.append(breakpad_file)
    return breakpad_files

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
    symbol_files = []
    so_file = os.path.join(self.test_build_dir, 'test_file.so')
    with open(so_file, 'w') as _:
      pass
    symbol_files.append(so_file)
    exe_file = os.path.join(self.test_build_dir, 'test_file.exe')
    with open(exe_file, 'w') as _:
      pass
    symbol_files.append(exe_file)
    chrome_file = os.path.join(self.test_build_dir, 'chrome')
    with open(chrome_file, 'w') as _:
      pass
    symbol_files.append(chrome_file)

    # Form output file paths.
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(
        side_effect=self._getDumpSymsMockSideEffect({}))
    breakpad_file_extractor.ExtractBreakpadFiles(self.test_dump_syms_binary,
                                                 self.test_build_dir,
                                                 self.test_breakpad_dir)

    # Check that each expected call to _RunDumpSyms() has been made.
    breakpad_files = self._getAndEnsureExtractedBreakpadFiles(symbol_files)
    expected_calls = self._getExpectedBreakpadExtractionCalls(
        symbol_files, breakpad_files)
    breakpad_file_extractor._RunDumpSyms.assert_has_calls(expected_calls,
                                                          any_order=True)

  def testDumpSymsNotFound(self):
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock()
    exception_msg = 'dump_syms binary not found.'
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
      file1.write(
          'MODULE Linux x86_64 34984AB4EF948C0000000000000000000 name1.so')

    did_extract = breakpad_file_extractor.ExtractBreakpadFiles(
        self.test_dump_syms_binary, self.test_build_dir, self.test_breakpad_dir)
    self.assertFalse(did_extract)

    os.remove(partition_file)

  def testIgnoreCombinedFiles(self):
    combined_file1 = os.path.join(self.test_build_dir, 'chrome_combined.so')
    combined_file2 = os.path.join(self.test_build_dir, 'libchrome_combined.so')
    with open(combined_file1, 'w') as file1:
      file1.write(
          'MODULE Linux x86_64 34984AB4EF948C0000000000000000000 name1.so')
    with open(combined_file2, 'w') as file2:
      file2.write(
          'MODULE Linux x86_64 34984AB4EF948C0000000000000000000 name2.so')

    did_extract = breakpad_file_extractor.ExtractBreakpadFiles(
        self.test_dump_syms_binary, self.test_build_dir, self.test_breakpad_dir)
    self.assertFalse(did_extract)

    os.remove(combined_file1)
    os.remove(combined_file2)

  def testExtractOnSubtree(self):
    # Setup subtree symbol files.
    symbol_files, side_effect_map = self._setupSubtreeFiles()
    subdir_symbols = symbol_files[0]
    unstripped_symbols = symbol_files[1]

    # Setup metadata.
    metadata = metadata_extractor.MetadataExtractor('trace_processor_shell',
                                                    'trace_file.proto')
    metadata.InitializeForTesting(
        modules={
            '/subdir.so': '34984AB4EF948D',
            '/unstripped.so': '34984AB4EF948C0000000000000000000'
        })
    extracted_files = [subdir_symbols, unstripped_symbols]

    # Setup |_RunDumpSyms| mock for module ID optimization.
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(
        side_effect=self._getDumpSymsMockSideEffect(side_effect_map))
    breakpad_file_extractor.ExtractBreakpadOnSubtree(self.test_breakpad_dir,
                                                     metadata,
                                                     self.test_dump_syms_binary)

    # Ensure correct |_RunDumpSyms| calls.
    expected_module_calls = self._getExpectedModuleExtractionCalls(symbol_files)

    breakpad_files = self._getAndEnsureExpectedSubtreeBreakpadFiles(
        extracted_files)
    expected_extract_calls = self._getExpectedBreakpadExtractionCalls(
        extracted_files, breakpad_files)

    breakpad_file_extractor._RunDumpSyms.assert_has_calls(
        expected_module_calls + expected_extract_calls, any_order=True)

  def testSubtreeNoFilesExtracted(self):
    # Setup subtree symbol files. No files to be extracted.
    symbol_files, side_effect_map = self._setupSubtreeFiles()

    # Empty set of module IDs to extract. Nothing should be extracted.
    metadata = metadata_extractor.MetadataExtractor('trace_processor_shell',
                                                    'trace_file.proto')
    metadata.InitializeForTesting(modules={})

    # Setup |_RunDumpSyms| mock for module ID optimization.
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(
        side_effect=self._getDumpSymsMockSideEffect(side_effect_map))
    exception_msg = (
        'No breakpad symbols could be extracted from files in the subtree: ' +
        self.test_breakpad_dir)
    with self.assertRaises(Exception) as e:
      breakpad_file_extractor.ExtractBreakpadOnSubtree(
          self.test_breakpad_dir, metadata, self.test_dump_syms_binary)
    self.assertIn(exception_msg, str(e.exception))

    # Should be calls to extract module ID, but none to extract breakpad.
    expected_module_calls = self._getExpectedModuleExtractionCalls(symbol_files)
    breakpad_file_extractor._RunDumpSyms.assert_has_calls(expected_module_calls,
                                                          any_order=True)

  def testFindOnSubtree(self):
    # Setup subtree symbol files.
    _, side_effect_map = self._setupSubtreeFiles()

    # Setup |_RunDumpSyms| mock for module ID optimization.
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(
        side_effect=self._getDumpSymsMockSideEffect(side_effect_map))

    found = get_symbols_util.FindMatchingModule(
        self.test_breakpad_dir, self.test_dump_syms_binary,
        '34984AB4EF948C0000000000000000000')
    self.assertIn('subdir.so', found)

  def testNotFindOnSubtree(self):
    # Setup subtree symbol files.
    _, side_effect_map = self._setupSubtreeFiles()

    # Setup |_RunDumpSyms| mock for module ID optimization.
    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(
        side_effect=self._getDumpSymsMockSideEffect(side_effect_map))

    found = get_symbols_util.FindMatchingModule(self.test_breakpad_dir,
                                                self.test_dump_syms_binary,
                                                'NOTFOUND')
    self.assertIsNone(found)


if __name__ == '__main__':
  unittest.main()
