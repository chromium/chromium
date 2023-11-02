# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to process compresssed files."""

import contextlib
import logging
import os
import pathlib
import re
import shutil
import struct
import tempfile
import zipfile


class _ApkFileManager:
  def __init__(self, temp_dir):
    self._temp_dir = pathlib.Path(temp_dir)
    self._subdir_by_apks_path = {}
    self._infolist_by_path = {}

  def _MapPath(self, path):
    # Use numbered subdirectories for uniqueness.
    # Suffix with basename(path) for readability.
    default = '-'.join(
        [str(len(self._subdir_by_apks_path)),
         os.path.basename(path)])
    return self._temp_dir / self._subdir_by_apks_path.setdefault(path, default)

  def InfoList(self, path):
    """Returns zipfile.ZipFile(path).infolist()."""
    ret = self._infolist_by_path.get(path)
    if ret is None:
      with zipfile.ZipFile(path) as z:
        ret = z.infolist()
      self._infolist_by_path[path] = ret
    return ret

  def SplitPath(self, minimal_apks_path, split_name):
    """Returns the path to the apk split extracted by ExtractSplits.

    Args:
      minimal_apks_path: The .apks file that was passed to ExtractSplits().
      split_name: Then name of the split.

    Returns:
      Path to the extracted .apk file.
    """
    subdir = self._subdir_by_apks_path[minimal_apks_path]
    if '-' in split_name:
      name = f'{split_name}.apk'
    else:
      name = f'{split_name}-master.apk'
    return self._temp_dir / subdir / 'splits' / name

  def ExtractSplits(self, minimal_apks_path):
    """Extracts the master splits in the given .apks file.

    Returns:
      List of split names, with "base" always appearing first.
    """
    dest = self._MapPath(minimal_apks_path)
    split_names = []
    logging.debug('Extracting %s', minimal_apks_path)
    with zipfile.ZipFile(minimal_apks_path) as z:
      for filename in z.namelist():
        # E.g.:
        # splits/base-master.apk
        # splits/base-hi.apk
        # splits/vr-master.apk
        # splits/vr-en.apk
        m = re.match(r'splits/(.*)-master\.apk', filename)
        if m:
          split_names.append(m.group(1))
          z.extract(filename, dest)
        # Also analyze -hi locale splits, since resource_sizes.py does this.
        m = re.match(r'splits/(.*-hi)\.apk', filename)
        if m:
          split_names.append(m.group(1))
          z.extract(filename, dest)
    logging.debug('Extracting %s (done)', minimal_apks_path)
    # Make "base" comes first since that's the main chunk of work.
    # Also so that --abi-filter detection looks at it first.
    return sorted(split_names, key=lambda x: (not x.startswith('base'), x))


@contextlib.contextmanager
def ApkFileManager():
  """Context manager that extracts apk splits to a temp dir."""
  # Cannot use tempfile.TemporaryDirectory() here because our use of
  # multiprocessing results in __del__ methods being called in forked processes.
  temp_dir = tempfile.mkdtemp(suffix='-supersize')
  zip_files = _ApkFileManager(temp_dir)
  yield zip_files
  shutil.rmtree(temp_dir)


@contextlib.contextmanager
def UnzipToTemp(zip_path, inner_path):
  """Extract a |inner_path| from a |zip_path| file to an auto-deleted temp file.

  Args:
    zip_path: Path to the zip file.
    inner_path: Path to the file within |zip_path| to extract.

  Yields:
    The path of the temp created (and auto-deleted when context exits).
  """
  try:
    logging.debug('Extracting %s', inner_path)
    _, suffix = os.path.splitext(inner_path)
    # Can't use NamedTemporaryFile() because it deletes via __del__, which will
    # trigger in both this and the fork()'ed processes.
    fd, temp_file = tempfile.mkstemp(suffix=suffix)
    with zipfile.ZipFile(zip_path) as z:
      os.write(fd, z.read(inner_path))
    os.close(fd)
    logging.debug('Extracting %s (done)', inner_path)
    yield temp_file
  finally:
    os.unlink(temp_file)


def ReadZipInfoExtraFieldLength(zip_file, zip_info):
  """Reads the value of |extraLength| from |zip_info|'s local file header.

  |zip_info| has an |extra| field, but it's read from the central directory.
  Android's zipalign tool sets the extra field only in local file headers.
  """
  # Refer to https://en.wikipedia.org/wiki/Zip_(file_format)#File_headers
  zip_file.fp.seek(zip_info.header_offset + 28)
  return struct.unpack('<H', zip_file.fp.read(2))[0]


def MeasureApkSignatureBlock(zip_file):
  """Measures the size of the v2 / v3 signing block.

  Refer to: https://source.android.com/security/apksigning/v2
  """
  # Seek to "end of central directory" struct.
  eocd_offset_from_end = -22 - len(zip_file.comment)
  zip_file.fp.seek(eocd_offset_from_end, os.SEEK_END)
  assert zip_file.fp.read(4) == b'PK\005\006', (
      'failed to find end-of-central-directory')

  # Read out the "start of central directory" offset.
  zip_file.fp.seek(eocd_offset_from_end + 16, os.SEEK_END)
  start_of_central_directory = struct.unpack('<I', zip_file.fp.read(4))[0]

  # Compute the offset after the last zip entry.
  last_info = max(zip_file.infolist(), key=lambda i: i.header_offset)
  last_header_size = (30 + len(last_info.filename) +
                      ReadZipInfoExtraFieldLength(zip_file, last_info))
  end_of_last_file = (last_info.header_offset + last_header_size +
                      last_info.compress_size)
  return start_of_central_directory - end_of_last_file
