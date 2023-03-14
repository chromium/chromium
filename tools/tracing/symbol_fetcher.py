# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts breakpad symbol file from Google Cloud Platform."""

import logging
import os
import zipfile

import breakpad_file_extractor
import flag_utils
from metadata_extractor import OSName
import py_utils.cloud_storage as cloud_storage
import rename_breakpad

ANDROID_X86_FOLDERS = {'x86', 'x86_64', 'next-x86', 'next-x86_64'}
ANDROID_ARM_FOLDERS = {'arm', 'arm_64', 'next-arm', 'next-arm_64'}
GCS_SYMBOLS = {
    'symbols.zip', 'Monochrome_symbols.zip', 'Monochrome_symbols-secondary.zip'
}


def GetTraceBreakpadSymbols(cloud_storage_bucket,
                            metadata,
                            breakpad_output_dir,
                            dump_syms_path=None):
  """Fetch trace symbols from GCS and convert to breakpad format, if needed.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    metadata: MetadataExtractor class that contains necessary trace file
      metadata for fetching its symbol file.
    breakpad_output_dir: local path to store trace symbol breakpad file.
    dump_syms_path: local path to dump_syms binary. Parameter required for
      official Android traces; not required for mac or linux traces.

  Raises:
    Exception: if failed to extract trace OS name or version number, or
      they are not supported or recognized.
  """
  metadata.Initialize()
  if metadata.os_name is None:
    raise Exception('Failed to extract trace OS name: ' + metadata.trace_file)
  if metadata.version_number is None:
    raise Exception('Failed to extract trace version number: ' +
                    metadata.trace_file)

  # Obtain breakpad symbols by platform.
  if metadata.os_name == OSName.ANDROID:
    GetAndroidSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
    breakpad_file_extractor.ExtractBreakpadOnSubtree(breakpad_output_dir,
                                                     metadata, dump_syms_path)
    rename_breakpad.RenameBreakpadFiles(breakpad_output_dir,
                                        breakpad_output_dir)
  elif metadata.os_name == OSName.WINDOWS:
    raise Exception(
        'Windows platform is not currently supported for symbolization.')
  elif metadata.os_name == OSName.LINUX or metadata.os_name == OSName.MAC:
    _FetchBreakpadSymbols(cloud_storage_bucket, metadata, breakpad_output_dir)
    rename_breakpad.RenameBreakpadFiles(breakpad_output_dir,
                                        breakpad_output_dir)
  else:
    raise Exception('Trace OS "%s" is not supported: %s' %
                    (metadata.os_name, metadata.trace_file))

  flag_utils.GetTracingLogger().info('Breakpad symbols located at: %s',
                                     os.path.abspath(breakpad_output_dir))


def GetAndroidSymbols(cloud_storage_bucket, metadata, breakpad_output_dir):
  """Fetches Android symbols from GCS.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    metadata: MetadataExtractor class that contains necessary trace file
      metadata for fetching its symbol file.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    Exception: if fails to extract architecture or version code from trace.
    RuntimeError: if fails to determine correct GCS folder.
  """
  if metadata.architecture is None:
    raise Exception('Failed to extract architecture: ' + metadata._trace_file)
  # Version code should exist for official builds.
  if metadata.version_code is None:
    raise Exception('Failed to extract version code: ' + metadata._trace_file)

  # Determine GCS folder.
  flag_utils.GetTracingLogger().debug('Determining Android GCS folder.')
  possible_arch_folders = set()
  if 'arm' in metadata.architecture:
    possible_arch_folders = ANDROID_ARM_FOLDERS
  else:
    possible_arch_folders = ANDROID_X86_FOLDERS

  gcs_folder = None
  for arch_folder in possible_arch_folders:
    possible_gcs_folder = ('android-B0urB0N/' + metadata.version_number + '/' +
                           arch_folder)
    # The correct folder's 'version_codes.txt' file, which contains all the
    # folder's Chrome release version codes, will match the trace's version
    # code.
    if _IsAndroidVersionCodeInFile(cloud_storage_bucket, metadata.version_code,
                                   possible_gcs_folder, breakpad_output_dir):
      gcs_folder = possible_gcs_folder
      break

  if gcs_folder is None:
    raise RuntimeError('Failed to determine architecture folder: ' +
                       str(metadata._trace_file))
  flag_utils.GetTracingLogger().debug(
      'Determined correct architecture folder is: %s', gcs_folder)

  # Fetch and unzip GCS symbol files.
  flag_utils.GetTracingLogger().info('Fetching Android symbols from GCS.')
  did_fetch_symbol_file = False
  for symbol in GCS_SYMBOLS:
    # Explicitly use backslashes for GCS paths to ensure that they are valid
    # on windows machines that use forward slashes. Local paths use python
    # |os.path.join| to utilize the correct slash for the system.
    gcs_symbol_file = gcs_folder + '/' + symbol
    symbol_zip_file = os.path.join(breakpad_output_dir, symbol)
    unzip_output_dir = os.path.join(breakpad_output_dir, symbol.split('.')[0])
    if not _FetchAndUnzipGCSFile(cloud_storage_bucket, gcs_symbol_file,
                                 symbol_zip_file, unzip_output_dir):
      flag_utils.GetTracingLogger().warning('Failed to find symbols on GCS: %s',
                                            gcs_symbol_file)
    else:
      did_fetch_symbol_file = True

  if not did_fetch_symbol_file:
    raise Exception('No symbol files could be found on GCS: ' + gcs_folder)


def _IsAndroidVersionCodeInFile(cloud_storage_bucket, version_code,
                                possible_gcs_folder, local_folder):
  """Determines if Android version code is in GCS 'version_codes.txt' file.

  The 'version_codes.txt' files contains all the version codes of the Chrome
  releases that were created from the current build directory. We determine
  the correct build directory to download symbols from by checking if the
  trace's version code matches any version code's in the build directory's
  'version_codes.txt' file. The trace's version code should uniquely match
  to one build directory.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    version_code: trace's version code.
    possible_gcs_folder: GCS build directory containing 'version_codes.txt'
      file.
    local_folder: local folder to download 'version_codes.txt' file to.

  Returns:
    True if version code in gcs folder's 'version_codes.txt' file;
    false otherwise.
  """
  gcs_version_code_file = possible_gcs_folder + '/version_codes.txt'
  local_version_code_file = os.path.join(local_folder, 'version_codes.txt')
  if not _FetchGCSFile(cloud_storage_bucket, gcs_version_code_file,
                       local_version_code_file):
    flag_utils.GetTracingLogger().debug(
        'Failed to download version code file: %s', gcs_version_code_file)
    return False

  with open(local_version_code_file, encoding='utf-8') as version_file:
    return version_code in version_file.read()


def _FetchBreakpadSymbols(cloud_storage_bucket, metadata, breakpad_output_dir):
  """Fetches and extracts Mac or Linux breakpad format symbolization file.

  Args:
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    metadata: MetadataExtractor class that contains necessary trace file
      metadata for fetching its symbol file.
    breakpad_output_dir: local path to store trace symbol breakpad file.

  Raises:
    Exception: if trace OS is not mac or linux, or failed to extract
      version number.
    ValueError: if linux trace is of 32 bit bitness.
  """
  # Determine GCS folder.
  folder = None
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
        flag_utils.GetTracingLogger().warning(
            'Architecture not found, so using x86-64.')
      folder = 'mac64'
  else:
    raise Exception('Expected OS "%s" to be Linux or Mac: %s' %
                    (metadata.os_name, metadata.trace_file))

  # Build Google Cloud Storage path to the symbols.
  assert folder is not None
  gcs_folder = 'desktop-*/' + metadata.version_number + '/' + folder
  gcs_file = gcs_folder + '/breakpad-info'
  gcs_zip_file = gcs_file + '.zip'

  # Local path to downloaded symbols.
  breakpad_zip = os.path.join(breakpad_output_dir, 'breakpad-info.zip')

  # Fetch and unzip symbol files from GCS. Some version, like mac,
  # don't have the .zip extension on GCS. Assumes that 'breakpad-info'
  # file (without .zip extension) from GCS is a zip file.
  if not _FetchAndUnzipGCSFile(cloud_storage_bucket, gcs_zip_file, breakpad_zip,
                               breakpad_output_dir):
    if not _FetchAndUnzipGCSFile(cloud_storage_bucket, gcs_file, breakpad_zip,
                                 breakpad_output_dir):
      raise Exception('Failed to find symbols on GCS: %s[.zip].' % (gcs_file))


def _FetchAndUnzipGCSFile(cloud_storage_bucket, gcs_file, gcs_output,
                          output_dir):
  """Fetch file from GCS to local |gcs_output|, then unzip it into |output_dir|.

  Returns:
    True if successfully fetches and unzips file; false, otherwise.

  Raises:
    zipfile.BadZipfile: if file is not a zip file
  """
  if _FetchGCSFile(cloud_storage_bucket, gcs_file, gcs_output):
    _UnzipFile(gcs_output, output_dir)
    return True
  return False


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
    flag_utils.GetTracingLogger().info('Downloading files from GCS: %s',
                                       gcs_file)
    cloud_storage.Get(cloud_storage_bucket, gcs_file, output_file)
    flag_utils.GetTracingLogger().info('Saved file locally to: %s', output_file)
    return True
  return False


def _UnzipFile(zip_file, output_dir):
  """Unzips file into provided output directory.

  Raises:
    zipfile.BadZipfile: if file is not a zip file
  """
  with zipfile.ZipFile(zip_file, 'r') as zip_f:
    zip_f.extractall(output_dir)
