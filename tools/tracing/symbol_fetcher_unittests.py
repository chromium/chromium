# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
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

  def tearDown(self):
    cloud_storage.Exists.reset_mock()
    cloud_storage.Get.reset_mock()

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
        Exception, ('Failed to find symbols on GCS: desktop-\*/123/linux64/'
                    'breakpad-info\[.zip\]')):
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


if __name__ == '__main__':
  unittest.main()
