# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to process compresssed files."""

import contextlib
import logging
import os
import struct
import tempfile
import zipfile


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
    _, suffix = os.path.splitext(inner_path)
    # Can't use NamedTemporaryFile() because it uses atexit, which does not play
    # well with fork().
    fd, temp_file = tempfile.mkstemp(suffix=suffix)
    logging.debug('Extracting %s', inner_path)
    with zipfile.ZipFile(zip_path) as z:
      os.write(fd, z.read(inner_path))
    os.close(fd)
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
  last_info = zip_file.infolist()[-1]
  last_header_size = (30 + len(last_info.filename) +
                      ReadZipInfoExtraFieldLength(zip_file, last_info))
  end_of_last_file = (last_info.header_offset + last_header_size +
                      last_info.compress_size)
  return start_of_central_directory - end_of_last_file
