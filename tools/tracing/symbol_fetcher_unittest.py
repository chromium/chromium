#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
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
from symbol_fetcher import ANDROID_X86_FOLDERS, ANDROID_ARM_FOLDERS, GCS_SYMBOLS
import breakpad_file_extractor
import rename_breakpad
import mock


class SymbolFetcherTestBase(unittest.TestCase):
  """Super class holding shared parameters and methods.

  All function stashing and unstashing happens here to ensure that tests
  do not mutate each other's functions. General mocks can be written here
  and overridden in respective subclasses or tests as needed.
  """

  def setUp(self):
    self.trace_processor_path = 'trace_processor_shell'
    self.trace_file = 'trace_file.proto'
    self.cloud_storage_bucket = 'chrome-unsigned'
    self.breakpad_output_dir = tempfile.mkdtemp()
    self.breakpad_zip_file = os.path.join(self.breakpad_output_dir,
                                          'breakpad-info.zip')
    self.local_version_code_file = os.path.join(self.breakpad_output_dir,
                                                'version_codes.txt')
    self.dump_syms_dir = tempfile.mkdtemp()
    self.dump_syms_path = os.path.join(self.dump_syms_dir, 'dump_syms')
    with open(self.dump_syms_path, 'w') as _:
      pass

    # Stash mocked functions.
    self.Exists_stash = cloud_storage.Exists
    self.Get_stash = cloud_storage.Get
    self.UnzipFile_stash = symbol_fetcher._UnzipFile
    self.FetchGCSFile_stash = symbol_fetcher._FetchGCSFile
    self.FetchAndUnzipGCSFile_stash = symbol_fetcher._FetchAndUnzipGCSFile
    self.RunDumpSyms_stash = breakpad_file_extractor._RunDumpSyms
    self.RenameBreakpadFiles_stash = rename_breakpad.RenameBreakpadFiles

    # Ignore this function. It is tested separately.
    rename_breakpad.RenameBreakpadFiles = mock.MagicMock()

  def tearDown(self):
    shutil.rmtree(self.breakpad_output_dir)
    shutil.rmtree(self.dump_syms_dir)

    # Unstash functions.
    cloud_storage.Exists = self.Exists_stash
    cloud_storage.Get = self.Get_stash
    symbol_fetcher._UnzipFile = self.UnzipFile_stash
    symbol_fetcher._FetchGCSFile = self.FetchGCSFile_stash
    symbol_fetcher._FetchAndUnzipGCSFile = self.FetchAndUnzipGCSFile_stash
    breakpad_file_extractor._RunDumpSyms = self.RunDumpSyms_stash
    rename_breakpad.RenameBreakpadFiles = self.RenameBreakpadFiles_stash

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

  def _ensureRenameCalled(self):
    rename_breakpad.RenameBreakpadFiles.assert_called_once_with(
        self.breakpad_output_dir, self.breakpad_output_dir)

  def testNoOSName(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=None,
                                             architecture='x86_64',
                                             bitness='64')

    exception_msg = 'Failed to extract trace OS name: ' + self.trace_file
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)
    self.assertIn(exception_msg, str(e.exception))

  def testOSNameNotRecognized(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name='blah',
                                             architecture='x86_64',
                                             bitness='64')

    exception_msg = 'Trace OS "blah" is not supported: ' + self.trace_file
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)
    self.assertIn(exception_msg, str(e.exception))

  def testNoVersionNumber(self):
    metadata = self._createMetadataExtractor(version_number=None,
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')

    exception_msg = 'Failed to extract trace version number: ' + self.trace_file
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)
    self.assertIn(exception_msg, str(e.exception))


class MacAndLinuxTestCase(SymbolFetcherTestBase):
  def setUp(self):
    super().setUp()

    cloud_storage.Get = mock.Mock()
    # Default: Cloud storage has |gcs_file|.zip file. We use |side_effect|
    # because tests that overwrite this will need to use |side_effect|.
    cloud_storage.Exists = mock.Mock(side_effect=[True])
    # Default: Ignore |_UnzipFile| function so we don't unzip fake files.
    symbol_fetcher._UnzipFile = mock.MagicMock()

  def _ensureUnzipAndRenameCalls(self):
    zip_file = os.path.join(self.breakpad_output_dir, 'breakpad-info.zip')
    symbol_fetcher._UnzipFile.assert_called_once_with(zip_file,
                                                      self.breakpad_output_dir)
    self._ensureRenameCalled()

  def testLinux(self):
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

    self._ensureUnzipAndRenameCalls()

  def testMacArm(self):
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

    self._ensureUnzipAndRenameCalls()

  def testMac86(self):
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

    self._ensureUnzipAndRenameCalls()

  def testMacNoArchitecture(self):
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

    self._ensureUnzipAndRenameCalls()

  def testMacNotZip(self):
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
    # |cloud_storage.Exists| called with |gcs_file|.zip, and with |gcs_file|
    # without the .zip extension.
    self.assertEqual(cloud_storage.Exists.call_count, 2)

    self._ensureUnzipAndRenameCalls()

  def testLinuxFetchFailure(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='64')
    # Cloud storage does not have |gcs_file| or |gcs_file|.zip extension.
    cloud_storage.Exists.side_effect = [False, False]

    with self.assertRaises(Exception):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)
    self.assertEqual(cloud_storage.Exists.call_count, 2)
    cloud_storage.Get.assert_not_called()

  def testLinux32BitFailure(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.LINUX,
                                             architecture='x86_64',
                                             bitness='32')

    with self.assertRaises(ValueError):
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir)

    cloud_storage.Exists.assert_not_called()
    cloud_storage.Get.assert_not_called()


class AndroidTestCase(SymbolFetcherTestBase):
  def setUp(self):
    super().setUp()

    # Base directories for Android symbols.
    self.out = tempfile.mkdtemp(dir=self.breakpad_output_dir)
    self.release = tempfile.mkdtemp(dir=self.out)
    self.subdir1 = tempfile.mkdtemp(dir=self.release)
    self.subdir2 = tempfile.mkdtemp(dir=self.release)
    self.unstripped_dir = os.path.join(self.subdir2, 'lib.unstripped')
    os.mkdir(self.unstripped_dir)

    breakpad_file_extractor._RunDumpSyms = mock.MagicMock(return_value=True)
    symbol_fetcher._FetchAndUnzipGCSFile = mock.MagicMock(return_value=True)

  def _setUpBasicRunDumpSyms(self):
    """Sets up symbol files to run the |RunDumpSyms| function.

    Basic file setup used across all (non-error) Android tests.
    """
    extracted_files = []
    extracted_files.append(os.path.join(self.unstripped_dir, 'unstripped.so'))
    extracted_files.append(os.path.join(self.subdir1, 'subdir1.so'))
    extracted_files.append(os.path.join(self.subdir2, 'subdir2.so'))
    extracted_files.append(os.path.join(self.release, 'release.so'))

    unextracted_files = []
    unextracted_files.append(
        os.path.join(self.breakpad_output_dir, 'monochrome_symbols.zip'))
    unextracted_files.append(
        os.path.join(self.breakpad_output_dir, 'version_codes.txt'))

    for new_file in extracted_files + unextracted_files:
      with open(new_file, 'w') as f:
        f.write('MODULE mac x86_64 329FDEA987BC name.so')

    return extracted_files

  def _ensureRunDumpSymsAndRenameCalls(self, extract_files):
    # Ensure |RunDumpSyms| calls.
    expected_calls = []
    for extract_file in extract_files:
      expected_call = mock.call(self.dump_syms_path, extract_file,
                                extract_file + '.breakpad')
      expected_calls.append(expected_call)

    breakpad_file_extractor._RunDumpSyms.assert_has_calls(expected_calls,
                                                          any_order=True)

    self._ensureRenameCalled()

  def _ensureRunDumpSymsAndRenameNotCalled(self):
    breakpad_file_extractor._RunDumpSyms.assert_not_called()
    rename_breakpad.RenameBreakpadFiles.assert_not_called()

  def _mockVersionCodeFetcher(self, match_arch_folder, metadata):
    """Sets GCS folder's 'version_codes.txt' file to match trace's version code.

    Fetching 'version_codes.txt' with the |match_arch_folder| will return a file
    that matches the trace's version code. All other 'version_codes.txt' file
    fetch calls return an empty file. If |match_arch_folder| is None, then no
    folder will match the trace's version code.

    Args:
      match_arch_folder: name of GCS architecture folder containing the
        'version_codes.txt' file that match the trace's version code.
      metadata: trace metadata information.
    """
    if match_arch_folder is None:
      version_code_path_to_match = None
    else:
      assert (match_arch_folder in ANDROID_ARM_FOLDERS
              or match_arch_folder in ANDROID_X86_FOLDERS)
      version_code_path_to_match = ('android-B0urB0N/' +
                                    metadata.version_number + '/' +
                                    match_arch_folder + '/version_codes.txt')

    def fetch_version_code_side_effect(*args):
      """Returns matching version code file for only the correct folder.
      """
      self.assertEqual(args[0], self.cloud_storage_bucket)
      args_version_code_path = args[1]
      self.assertFalse(args_version_code_path is None)
      self.assertEqual(args[2], self.local_version_code_file)

      with open(self.local_version_code_file, 'w') as version_file:
        if args_version_code_path == version_code_path_to_match:
          version_file.write(metadata.version_code)
        else:
          version_file.write('')
      return True

    return fetch_version_code_side_effect

  def _getExpectedSymbolFileFetches(self, match_arch_folder, metadata):
    """Builds the expected calls to the |_FetchAndUnzipGCSFile| function.

    Args:
      match_arch_folder: name of GCS architecture folder containing the
        'version_codes.txt' file that match the trace's version code.
      metadata: trace metadata information.
    """
    # We only fetch symbol files from the folder that matches the trace's
    # version code.
    path_to_match = ('android-B0urB0N/' + metadata.version_number + '/' +
                     match_arch_folder)

    expected_calls = []
    for symbol in GCS_SYMBOLS:
      gcs_symbol_file = path_to_match + '/' + symbol
      symbol_zip_file = os.path.join(self.breakpad_output_dir, symbol)
      unzip_output_dir = os.path.join(self.breakpad_output_dir,
                                      symbol.split('.')[0])
      symbol_call = mock.call(self.cloud_storage_bucket, gcs_symbol_file,
                              symbol_zip_file, unzip_output_dir)
      expected_calls.append(symbol_call)

    return expected_calls

  def testAndroid(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    extract_files = self._setUpBasicRunDumpSyms()

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir,
                                           self.dump_syms_path)

    expected_calls = self._getExpectedSymbolFileFetches(match_arch_folder,
                                                        metadata)
    symbol_fetcher._FetchAndUnzipGCSFile.assert_has_calls(expected_calls,
                                                          any_order=True)
    self._ensureRunDumpSymsAndRenameCalls(extract_files)

  def testCrossArchitecture(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'next-x86'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    extract_files = self._setUpBasicRunDumpSyms()

    symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket, metadata,
                                           self.breakpad_output_dir,
                                           self.dump_syms_path)

    expected_calls = self._getExpectedSymbolFileFetches(match_arch_folder,
                                                        metadata)
    symbol_fetcher._FetchAndUnzipGCSFile.assert_has_calls(expected_calls,
                                                          any_order=True)
    self._ensureRunDumpSymsAndRenameCalls(extract_files)

  def testMissingArchitecture(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture=None,
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    self._setUpBasicRunDumpSyms()

    exception_msg = 'Failed to extract architecture: ' + self.trace_file
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))
    self._ensureRunDumpSymsAndRenameNotCalled()

  def testMissingVersionCode(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code=None,
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    self._setUpBasicRunDumpSyms()

    exception_msg = 'Failed to extract version code: ' + self.trace_file
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))
    self._ensureRunDumpSymsAndRenameNotCalled()

  def testMissingDumpsymsPath(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='328954',
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    self._setUpBasicRunDumpSyms()

    exception_msg = ('Path to dump_syms binary is required for symbolizing '
                     'official Android traces.')
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata,
                                             self.breakpad_output_dir,
                                             dump_syms_path=None)
    self.assertIn(exception_msg, str(e.exception))
    self._ensureRunDumpSymsAndRenameNotCalled()

  def testFailsToFetchAllVersionCodesFiles(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)

    # Fails to fetch all 'version_codes.txt' files from GCS.
    symbol_fetcher._FetchGCSFile = mock.MagicMock(return_value=False)
    self._setUpBasicRunDumpSyms()

    exception_msg = ('Failed to determine architecture folder: ' +
                     self.trace_file)
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))
    self._ensureRunDumpSymsAndRenameNotCalled()

  def testNoFilesMatchTraceVersionCode(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    # None of the 'version_codes.txt' files match the trace's version code.
    match_arch_folder = None  # No valid paths can be None.
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    self._setUpBasicRunDumpSyms()

    exception_msg = ('Failed to determine architecture folder: ' +
                     self.trace_file)
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))
    self._ensureRunDumpSymsAndRenameNotCalled()

  def testFailsToFetchSymbolFiles(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='armv7',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'next-arm_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    # Fails to fetch all symbol files from GCS.
    symbol_fetcher._FetchAndUnzipGCSFile = mock.MagicMock(return_value=False)
    self._setUpBasicRunDumpSyms()

    gcs_folder = ('android-B0urB0N/' + metadata.version_number + '/' +
                  match_arch_folder)
    exception_msg = 'No symbol files could be found on GCS: ' + gcs_folder
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))

    # Expect to try to download all symbol files.
    expected_calls = self._getExpectedSymbolFileFetches(match_arch_folder,
                                                        metadata)
    symbol_fetcher._FetchAndUnzipGCSFile.assert_has_calls(expected_calls,
                                                          any_order=True)

    self._ensureRunDumpSymsAndRenameNotCalled()

  def testNoSymbolsExtracted(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))
    # Don't run |self._setUpBasicRunDumpSyms()| because we want no symbols
    # to be extracted.

    exception = (
        'No breakpad symbols could be extracted from files in the subtree: ' +
        self.breakpad_output_dir)
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception, str(e.exception))
    expected_calls = self._getExpectedSymbolFileFetches(match_arch_folder,
                                                        metadata)
    symbol_fetcher._FetchAndUnzipGCSFile.assert_has_calls(expected_calls,
                                                          any_order=True)

    self._ensureRunDumpSymsAndRenameNotCalled()

  def testIgnoreInvalidSymbolFiles(self):
    metadata = self._createMetadataExtractor(version_number='123',
                                             os_name=OSName.ANDROID,
                                             architecture='x86_64',
                                             bitness='64',
                                             version_code='358923',
                                             modules=None)
    match_arch_folder = 'x86_64'
    symbol_fetcher._FetchGCSFile = mock.Mock(
        side_effect=self._mockVersionCodeFetcher(match_arch_folder, metadata))

    # Setup invalid symbol files. No breakpad files should be extracted.
    zip_file = os.path.join(self.subdir1, 'monochrome_symbols.zip')
    apk_file = os.path.join(self.subdir2, 'chrome_apk')
    dwp_file = os.path.join(self.unstripped_dir, 'chrome.so.dwp')
    dwo_file = os.path.join(self.release, 'name.so.dwo')
    version_file = os.path.join(self.breakpad_output_dir, 'version_codes.txt')

    unextract_files = {zip_file, apk_file, dwp_file, dwo_file, version_file}
    for new_file in unextract_files:
      with open(new_file, 'w') as f:
        f.write('MODULE mac x86_64 329FDEA987BC name.so')

    exception_msg = (
        'No breakpad symbols could be extracted from files in the subtree: ' +
        self.breakpad_output_dir)
    with self.assertRaises(Exception) as e:
      symbol_fetcher.GetTraceBreakpadSymbols(self.cloud_storage_bucket,
                                             metadata, self.breakpad_output_dir,
                                             self.dump_syms_path)
    self.assertIn(exception_msg, str(e.exception))
    expected_calls = self._getExpectedSymbolFileFetches(match_arch_folder,
                                                        metadata)
    symbol_fetcher._FetchAndUnzipGCSFile.assert_has_calls(expected_calls,
                                                          any_order=True)

    # There should be no extracted files.
    self._ensureRunDumpSymsAndRenameNotCalled()


if __name__ == '__main__':
  unittest.main()
