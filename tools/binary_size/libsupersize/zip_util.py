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
