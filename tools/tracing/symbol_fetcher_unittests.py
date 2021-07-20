# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import shutil
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

import py_utils.cloud_storage as cloud_storage
import metadata_extractor
from metadata_extractor import OSName
import symbol_fetcher
import mock


class SymbolFetcherTestCase(unittest.TestCase):
  def setUp(self):
    self.trace_processor_path = 'trace_processor_shell'
    self.trace_file = 'trace_file.proto'
    self.cloud_storage_bucket = 'chrome-unsigned'
    self.breakpad_output_dir = 'breakpad-output-dir'
    self.breakpad_zip_file = self.breakpad_output_dir + '/breakpad-info.zip'
    cloud_storage.Exists = mock.Mock()
    cloud_storage.Get = mock.Mock()

    # Default: Cloud storage has |gcs_file| + .zip extension.
    # We use |side_effect| because tests that overwrite this
    # will need to use |side_effect|.
    cloud_storage.Exists.side_effect = [True]

    # Default: Ignore |_UnzipAndRenameBreakpadFiles| function
    # so we don't unzip fake files.
    symbol_fetcher._UnzipAndRenameBreakpadFiles = mock.MagicMock(
        side_effect=self._emptyFunc)

  def tearDown(self):
    cloud_storage.Exists.reset_mock()
    cloud_storage.Get.reset_mock()

  def _emptyFunc(*args):
    return

  def _createMetadataExtractor(self,
                               version_number=None,
                               os_name=None,
                               architecture=None,
                               bitness=None,
                               version_code=None,
                               modules=None):
    metadata = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                    self.trace_file)
    metadata.InitializeForTesting(version_number, os_name, architecture,
                                  bitness, version_code, modules)
    return metadata

  def testSymbolFetcherLinux(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Get.assert_called_once_with(
        self.cloud_storage_bucket, 'desktop-*/123/linux64/breakpad-info.zip',
        self.breakpad_zip_file)
    cloud_storage.Exists.assert_called_once()

  def testSymbolFetcherMacArm(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.MAC,
                                             architecture='armv7l',
                                             bitness='64')

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Get.assert_called_once_with(
        self.cloud_storage_bucket, 'desktop-*/123/mac-arm64/breakpad-info.zip',
        self.breakpad_zip_file)
    cloud_storage.Exists.assert_called_once()

  def testSymbolFetcherMac86(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.MAC,
                                             architecture='x86_64',
                                             bitness='64')

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Get.assert_called_once_with(
        self.cloud_storage_bucket, 'desktop-*/123/mac64/breakpad-info.zip',
        self.breakpad_zip_file)
    cloud_storage.Exists.assert_called_once()

  def testSymbolFetcherMacNoArchitecture(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.MAC,
                                             architecture=None,
                                             bitness='64')

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Get.assert_called_once_with(
        self.cloud_storage_bucket, 'desktop-*/123/mac64/breakpad-info.zip',
        self.breakpad_zip_file)
    cloud_storage.Exists.assert_called_once()

  def testSymbolFetcherMacNotZip(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.MAC,
                                             architecture='x86_64',
                                             bitness='64')
    # Cloud storage has |gcs_file| without a .zip extension.
    cloud_storage.Exists.side_effect = [False, True]

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Get.assert_called_once_with(
        self.cloud_storage_bucket, 'desktop-*/123/mac64/breakpad-info',
        self.breakpad_zip_file)
    # |cloud_storage.Exists| called with |gcs_file| with .zip extension,
    # and with |gcs_file| without the .zip extension.
    self.assertEqual(cloud_storage.Exists.call_count, 2)

  def testSymbolLinuxFetchFailure(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')
    # Cloud storage does not have |gcs_file| or |gcs_file| with .zip extension.
    cloud_storage.Exists.side_effect = [False, False]

    with self.assertRaisesRegex(
        Exception, ('Failed to find symbols on GCS: desktop-\\*/123/linux64/'
                    'breakpad-info\\[.zip\\]')):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)
    self.assertEqual(cloud_storage.Exists.call_count, 2)
    cloud_storage.Get.assert_not_called()

  def testSymbolFetcherNoOSName(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=None,
                                             architecture='x86_64',
                                             bitness='64')

    with self.assertRaisesRegex(Exception, 'Failed to extract trace OS name.'):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()

  def testSymbolFetcherOSNameNotRecognized(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name='blah',
                                             architecture='x86_64',
                                             bitness='64')

    with self.assertRaisesRegex(Exception, 'Trace OS is not supported: blah'):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()

  def testSymbolFetcherLinux32Bit(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='32')

    with self.assertRaisesRegex(ValueError,
                                '32 bit Linux traces are not supported.'):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()

  def testSymbolFetcherNoVersionNumber(self):
    metadata = self._createMetadataExtractor(version_number=None,
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')

    with self.assertRaisesRegex(Exception,
                                'Failed to extract trace version number.'):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()

  def testFetchBreakpadSymbolsWrongOS(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64')

    with self.assertRaisesRegex(Exception,
                                'Expected OS to be Linux or Mac: Android'):
      symbol_fetcher._FetchBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()

  def testUnzipAndRenameBreakpadFiles(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir)

    symbol_fetcher._UnzipAndRenameBreakpadFiles.assert_called_once_with(
        self.breakpad_zip_file, self.breakpad_output_dir)


class RenameBreakpadFilesTestCase(unittest.TestCase):
  def setUp(self):
    self.breakpad_dir = tempfile.mkdtemp()
    self.breakpad_output_dir = tempfile.mkdtemp()

    # Directory to unzipped breakpad files.
    self.breakpad_unzip_dir = tempfile.mkdtemp(dir=self.breakpad_dir)

  def tearDown(self):
    shutil.rmtree(self.breakpad_dir)
    shutil.rmtree(self.breakpad_output_dir)

  def _listSubtree(self, root):
    """Returns absolute paths of files and dirs in root subtree.
    """
    files = set()
    dirs = set()
    for root, subdirs, filenames in os.walk(root):
      for filename in filenames:
        files.add(os.path.join(root, filename))
      for subdir in subdirs:
        dirs.add(os.path.join(root, subdir))
    return dirs, files

  def _assertFilesInInputDir(self,
                             expected_unmoved_files=frozenset(),
                             expected_unmoved_dirs=frozenset()):
    """Ensures that |RenameBreakpadFiles| doesn't move files/dirs.

    Automatically adds |self.breakpad_unzip_dir| to
    |expected_unmoved_dirs| since this file should never be moved.
    """
    breakpad_unzip_dir = {self.breakpad_unzip_dir}
    expected_unmoved_dirs = expected_unmoved_dirs.union(breakpad_unzip_dir)

    unmoved_dirs, unmoved_files = self._listSubtree(self.breakpad_dir)
    self.assertEqual(expected_unmoved_files, unmoved_files)
    self.assertEqual(expected_unmoved_dirs, unmoved_dirs)

  def _assertFilesInOutputDir(self, expected_moved_files=frozenset()):
    """Ensures that |RenameBreakpadFiles| correctly moves files.
    """
    moved_files = set(os.listdir(self.breakpad_output_dir))
    self.assertEqual(expected_moved_files, moved_files)

  def testRenameBreakpadFiles(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'34984AB4EF948C.breakpad', '29E6F9A7CE00F.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesUpperCaseName(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 12a345b6c7def890 name1.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'12A345B6C7DEF890.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesLinuxx86_64(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad.x64')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'34984AB4EF948C.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesMultipleSubdirs(self):
    subdir1 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    subdir2 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(subdir1, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir2, 'file2.breakpad')
    breakpad_file3 = os.path.join(self.breakpad_unzip_dir, 'file3.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 48537ABD name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 38ABC9F name2.so')
    with open(breakpad_file3, 'w') as file3:
      file3.write('MODULE mac x86_64 45DFE name3.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_dirs = {subdir1, subdir2}
    self._assertFilesInInputDir(expected_unmoved_dirs=expected_unmoved_dirs)

    expected_moved_files = {
        '48537ABD.breakpad', '38ABC9F.breakpad', '45DFE.breakpad'
    }
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesNonBreakpad(self):
    valid_file = os.path.join(self.breakpad_unzip_dir, 'valid.breakpad')
    fake_file = os.path.join(self.breakpad_unzip_dir, 'fake.gz')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.json')
    random_dir = tempfile.mkdtemp(dir=self.breakpad_dir)
    non_breakpad_file = os.path.join(random_dir, 'random.txt')

    with open(valid_file, 'w') as file1:
      file1.write('MODULE mac x86_64 329FDEA987BC name.so')
    with open(fake_file, 'w') as file2:
      file2.write('random text blah blah blah')
    with open(empty_file, 'w'):
      pass
    with open(non_breakpad_file, 'w') as file3:
      file3.write('MODULE mac x86_64 329FDEA987BC name.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {fake_file, empty_file, non_breakpad_file}
    expected_unmoved_dirs = {random_dir}
    self._assertFilesInInputDir(expected_unmoved_files, expected_unmoved_dirs)

    expected_moved_files = {'329FDEA987BC.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesInvalidBreakpad(self):
    valid_file = os.path.join(self.breakpad_unzip_dir, 'valid.breakpad')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.breakpad')
    no_module_file = os.path.join(self.breakpad_unzip_dir, 'no-module.breakpad')
    short_file = os.path.join(self.breakpad_unzip_dir, 'short.breakpad')

    with open(valid_file, 'w') as file1:
      file1.write('MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name')
    with open(empty_file, 'w'):
      pass
    with open(no_module_file, 'w') as file2:
      file2.write('NOTMODULE mac x86_64 1240DF90E9AC39038EF400 name')
    with open(short_file, 'w') as file3:
      file3.write('MODULE mac 1240DF90E9AC39038EF400 name')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {empty_file, no_module_file, short_file}
    self._assertFilesInInputDir(expected_unmoved_files)

    expected_moved_files = {'1240DF90E9AC39038EF400.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesOnlyNonBreakpadAndMisformat(self):
    fake_file = os.path.join(self.breakpad_unzip_dir, 'fake.breakpad')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.breakpad')
    no_module_file = os.path.join(self.breakpad_unzip_dir, 'no-module.breakpad')
    short_file = os.path.join(self.breakpad_unzip_dir, 'short.breakpad')
    random_dir = tempfile.mkdtemp(dir=self.breakpad_dir)
    non_breakpad_file = os.path.join(random_dir, 'random.txt')

    with open(fake_file, 'w') as file1:
      file1.write('random text blah blah blah')
    with open(empty_file, 'w'):
      pass
    with open(no_module_file, 'w') as file2:
      file2.write('NOTMODULE mac x86_64 1240DF90E9AC39038EF400 name')
    with open(short_file, 'w') as file3:
      file3.write('MODULE mac 1240DF90E9AC39038EF400 name')
    with open(non_breakpad_file, 'w') as file4:
      file4.write('MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {
        fake_file, non_breakpad_file, empty_file, no_module_file, short_file
    }
    expected_unmoved_dirs = {random_dir}
    self._assertFilesInInputDir(expected_unmoved_files, expected_unmoved_dirs)

    self._assertFilesInOutputDir()

  def testRenameBreakpadFilesRepeatedModuleID(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE mac x86_64 12ABC8987DE name.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 12ABC8987DE name.so')

    # Build regex to check. Order of file printing doesn't matter,
    # but one file must be the original file and one file must be
    # the module name file.
    module_name = os.path.join(self.breakpad_output_dir, '12ABC8987DE.breakpad')
    file_regex = '(({bf1}|{bf2}), {mod})|({mod}, ({bf1}|{bf2}))'.format(
        bf1=breakpad_file1, bf2=breakpad_file2, mod=module_name)
    assertion_regex = ('Symbol file modules ids are not unique: 12ABC8987DE'
                       '\nSee these files: ' + file_regex)

    with self.assertRaisesRegex(AssertionError, assertion_regex):
      symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                          self.breakpad_output_dir)

    # Check breakpad file with repeated module id is not moved. More
    # complicated because either of the breakpad files could be moved.
    self.assertTrue(
        os.path.isfile(breakpad_file1) ^ os.path.isfile(breakpad_file2))

    # Ensure one of the breakpad module files got moved. No matter which
    # breakpad file got moved, it will have the same new module-based filename.
    expected_moved_files = {'12ABC8987DE.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesRepeatedModuleIDMultipleSubdirs(self):
    subdir1 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    subdir2 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(subdir1, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir2, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE mac x86_64 ABCE4853004895 name.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 ABCE4853004895 name.so')

    # Build regex to check. Order of file printing doesn't matter,
    # but one file must be the original file and one file must be
    # the module name file.
    module_name = os.path.join(self.breakpad_output_dir,
                               'ABCE4853004895.breakpad')
    file_regex = '(({bf1}|{bf2}), {mod})|({mod}, ({bf1}|{bf2}))'.format(
        bf1=breakpad_file1, bf2=breakpad_file2, mod=module_name)
    assertion_regex = ('Symbol file modules ids are not unique: ABCE4853004895'
                       '\nSee these files: ' + file_regex)

    with self.assertRaisesRegex(AssertionError, assertion_regex):
      symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                          self.breakpad_output_dir)

    # Check breakpad file with repeated module id is not moved. More
    # complicated because either of the breakpad files could be moved.
    self.assertTrue(
        os.path.isfile(breakpad_file1) ^ os.path.isfile(breakpad_file2))

    # Ensure one of the breakpad module files got moved. No matter which
    # breakpad file got moved, it will have the same new module-based filename.
    expected_moved_files = {'ABCE4853004895.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesInputDirEqualsOutputDir(self):
    subdir = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir, self.breakpad_dir)

    # All files should be moved into |self.breakpad_dir|.
    moved_breakpad1 = os.path.join(self.breakpad_dir, '34984AB4EF948C.breakpad')
    moved_breakpad2 = os.path.join(self.breakpad_dir, '29E6F9A7CE00F.breakpad')
    expected_files = {moved_breakpad1, moved_breakpad2}
    expected_unmoved_dirs = {subdir}
    self._assertFilesInInputDir(expected_files, expected_unmoved_dirs)

    # No files should be moved to |self.breakpad_output_dir|.
    self._assertFilesInOutputDir()

  def testRenameBreakpadFilesAllUnmoved(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    symbol_fetcher._RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_unzip_dir)

    # All files should be renamed but not moved.
    unmoved_breakpad1 = os.path.join(self.breakpad_unzip_dir,
                                     '34984AB4EF948C.breakpad')
    unmoved_breakpad2 = os.path.join(self.breakpad_unzip_dir,
                                     '29E6F9A7CE00F.breakpad')
    expected_renamed_files = {unmoved_breakpad1, unmoved_breakpad2}
    self._assertFilesInInputDir(expected_renamed_files)

    # No files should be moved to |self.breakpad_output_dir|.
    self._assertFilesInOutputDir()


if __name__ == '__main__':
  unittest.main()
