# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Extracts breakpad symbol file from Google Cloud Platform.
"""

import os
import sys
import logging

import py_utils.cloud_storage as cloud_storage
import metadata_extractor
from metadata_extractor import OSName


def GetTraceBreakpadSymbols(cloud_storage_bucket, metadata,
                            breakpad_output_dir):
  """Fetch trace symbols from GCS and convert to breakpad format, if needed.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    metadata: MetadataExtractor class that contains necessary
      trace file metadata for fetching its symbol file.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    Exception: if failed to extract trace OS name or version number, or
      they are not supported or recognized.
  """
  # TODO(rhuckleberry): Cache symbols so we do not have to download symbols
  # from GCS for every trace.

  metadata.Initialize()
  if metadata.os_name is None:
    raise Exception('Failed to extract trace OS name.')
  if metadata.version_number is None:
    raise Exception('Failed to extract trace version number.')

  # Extract symbols by platform.
  if metadata.os_name == OSName.ANDROID:
    _FetchAndroidSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  elif metadata.os_name == OSName.WINDOWS:
    _FetchWindowsSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  elif metadata.os_name == OSName.LINUX or metadata.os_name == OSName.MAC:
    _FetchBreakpadSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  else:
    raise Exception('Trace OS is not supported: ' + metadata.os_name)

  logging.info('Breakpad symbols located at: ' +
               os.path.abspath(breakpad_output_dir))


def _FetchAndroidSymbols(cloud_storage_bucket, metadata, breakpad_output_dir):
  """Fetch and extract Android symbolization file.
  """
  # TODO(rhuckleberry): Implement android fetching.
  raise Exception('Android platform is not currently supported.')


def _FetchBreakpadSymbols(cloud_storage_bucket, metadata, breakpad_output_dir):
  """Fetch and extract Mac or Linux breakpad format symbolization file.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    metadata: MetadataExtractor class that contains necessary
      trace file metadata for fetching its symbol file.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    Exception: if trace OS is not mac or linux, or failed to extract
      version number.
    ValueError: if linux trace is of 32 bit bitness.
  """
  # Determine GCS folder.
  if metadata.os_name == OSName.LINUX:
    if metadata.bitness == '32':
      raise ValueError('32 bit Linux traces are not supported.')
    folder = 'linux64'
  elif metadata.os_name == OSName.MAC:
    if (metadata.architecture is
        not None) and 'arm' in metadata.architecture.lower():
      folder = 'mac-arm64'
    else:
      if metadata.architecture is None:
        logging.warning('Architecture not found, so using x86-64.')
      folder = 'mac64'
  else:
    raise Exception('Expected OS to be Linux or Mac: ' + metadata.os_name)

  # Build Google Cloud Storage path to the symbols.
  gsc_folder = 'desktop-*/' + metadata.version_number + '/' + folder
  gcs_file = gsc_folder + '/breakpad-info'
  gcs_zip_file = gcs_file + '.zip'

  # Local path to downloaded symbols.
  breakpad_zip_file = breakpad_output_dir + '/breakpad-info.zip'

  # Fetch symbol files from GCS.
  # Some version, like mac, don't have the .zip extension on GCS.
  if not _FetchGCSFile(cloud_storage_bucket, gcs_zip_file, breakpad_zip_file):
    if not _FetchGCSFile(cloud_storage_bucket, gcs_file, breakpad_zip_file):
      raise Exception('Failed to find symbols on GCS: ' + gcs_file + '[.zip]')

  _UnzipAndRenameBreakpadFiles(breakpad_zip_file, breakpad_output_dir)


def _FetchWindowsSymbols(cloud_storage_bucket, metadata, breakpad_output_dir):
  """Fetch and extract Windows symbolization file.
  """
  # TODO(rhuckleberry): Implement windows fetching.
  raise Exception('Windows platform is not currently supported.')


def _FetchGCSFile(cloud_storage_bucket, gcs_file, output_file):
  """Fetch and save file from GCS to |output_file|.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    gcs_file: path to file in GCS.
    output_file: local file to store fetched GCS file.

  Returns:
    True if successfully fetches file; False, otherwise.
  """
  if cloud_storage.Exists(cloud_storage_bucket, gcs_file):
    logging.info('Downloading files from GCS: ' + gcs_file)
    cloud_storage.Get(cloud_storage_bucket, gcs_file, output_file)
    logging.info('Saved file locally to: ' + output_file)
    return True
  return False


def _UnzipAndRenameBreakpadFiles(breakpad_zip_file, breakpad_output_dir):
  """Unzips and renames breakpad files.

  Args:
    breakpad_zip_file: local symbol zip file.
    breakpad_output_dir: local path to store trace symbol breakpad file.
  """
  # TODO(rhuckleberry): Unzip breakpad file.

  # TODO(rhuckleberry): Rename breakpad files.
  pass
