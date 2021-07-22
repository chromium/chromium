# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Extracts breakpad symbol file from Google Cloud Platform.
"""

import os
import sys
import zipfile
import logging
import shutil

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
    raise Exception('Failed to extract trace OS name: ' + metadata.trace_file)
  if metadata.version_number is None:
    raise Exception('Failed to extract trace version number: ' +
                    metadata.trace_file)

  # Extract symbols by platform.
  if metadata.os_name == OSName.ANDROID:
    _FetchAndroidSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  elif metadata.os_name == OSName.WINDOWS:
    _FetchWindowsSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  elif metadata.os_name == OSName.LINUX or metadata.os_name == OSName.MAC:
    _FetchBreakpadSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
  else:
    raise Exception('Trace OS "%s" is not supported: %s' %
                    (metadata.os_name, metadata.trace_file))

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
    raise Exception('Expected OS "%s" to be Linux or Mac: %s' %
                    (metadata.os_name, metadata.trace_file))

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

  # Assumes that 'breakpad-info' file (without .zip extension) from GCS
  # is a zip file.
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
  # Unzip the breakpad file to |breakpad_dir|.
  breakpad_dir = os.path.dirname(breakpad_zip_file)
  with zipfile.ZipFile(breakpad_zip_file, 'r') as zip_file:
    zip_file.extractall(breakpad_dir)

  # Rename breakpad files.
  _RenameBreakpadFiles(breakpad_output_dir, breakpad_output_dir)


def _RenameBreakpadFiles(breakpad_dir, breakpad_output_dir):
  """Move breakpad files to new directory and rename them.

  Breakpad files (files that contain '.breakpad') are renamed
  to follow a '<module_id>.breakpad' with upper-case hexadecimal
  naming scheme and moved to the |breakpad_output_dir| directory.
  See perfetto::profiling::BreakpadSymbolizer for more naming
  information. All other non-breakpad or misformatted breakpad files
  remain in the same directory with the same filename.

  Args:
    breakpad_dir: local directory that stores symbol files in its subtree.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    AssertionError: if a file's module_id is None or repeated by
      another file.
  """
  # Runs on every directory in the subtree. Scans directories from top-down
  # (root to leaves) so that we don't rename files multiple times in the common
  # case where |breakpad_dir| = |breakpad_output_dir|.
  for subdir_path, _, filenames in os.walk(breakpad_dir, topdown=True):
    for filename in filenames:
      file_path = os.path.abspath(os.path.join(subdir_path, filename))

      if not '.breakpad' in filename:
        logging.debug("File is not a breakpad file: " + file_path)
        continue

      module_id = _ExtractModuleIdIfValidBreakpad(file_path)
      if module_id is None:
        logging.debug("Failed to extract file module id: " + file_path)
        continue

      new_filename = module_id + '.breakpad'
      dest_path = os.path.abspath(
          os.path.join(breakpad_output_dir, new_filename))

      # Ensure all new filenames (module ids) are unique. If there is module id
      # repetition, the first file with the same module_id has already been
      # moved.
      if os.path.exists(dest_path):
        raise AssertionError(('Symbol file modules ids are not '
                              'unique: %s\nSee these files: %s, %s' %
                              (module_id, file_path, dest_path)))

      shutil.move(file_path, dest_path)

  # TODO(rhuckleberry): After moving breakpad files we can be left with empty
  # dirs. Clean up these empty dirs if user specifies |breakpad_output_dir|.
  # Doesn't matter if |breakpad_output_dir| is a temporary directory.


def _ExtractModuleIdIfValidBreakpad(file_path):
  """Extracts breakpad file's module id if the file is valid.

  A breakpad file is valid for extracting its module id if it
  has a valid MODULE record, formatted like so:
  MODULE operatingsystem architecture id name

  For example:
  MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name

  See this for more information:
  https://chromium.googlesource.com/breakpad/breakpad/+/HEAD/docs/symbol_files.md#records-1

  Args:
    file_path: Path to breakpad file to extract module id from.

  Returns:
    Module id if file is a valid breakpad file; None, otherwise.
  """
  module_id = None
  with open(file_path, 'r') as file_handle:
    # Reads a maximum of 200 bytes/characters. Malformed file or binary will
    # not have '\n' character.
    first_line = file_handle.readline(200)
    fragments = first_line.rstrip().split()
    if fragments and fragments[0] == 'MODULE' and len(fragments) >= 5:
      # Symbolization script's input file format requires module id hexadecimal
      # to be upper case.
      module_id = fragments[3].upper()

  return module_id
