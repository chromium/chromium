# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for dealing with .zip files.

Copied from
https://source.chromium.org/chromium/chromium/src/+/main:build/zip_helpers.py;drc=86180695506584e8226a4ed14259e6f0eceabe4e
"""

import os
import pathlib
import posixpath
import stat
import time
import zipfile


def _hermetic_date_time(timestamp=None):
  if not timestamp:
    return (2001, 1, 1, 0, 0, 0)
  utc_time = time.gmtime(timestamp)
  return (utc_time.tm_year, utc_time.tm_mon, utc_time.tm_mday, utc_time.tm_hour,
          utc_time.tm_min, utc_time.tm_sec)


def add_to_zip_hermetic(zip_file,
                        zip_path,
                        *,
                        src_path=None,
                        data=None,
                        compress=None,
                        timestamp=None):
  """Adds a file to the given ZipFile with a hard-coded modified time.

  Args:
    zip_file: ZipFile instance to add the file to.
    zip_path: Destination path within the zip file (or ZipInfo instance).
    src_path: Path of the source file. Mutually exclusive with |data|.
    data: File data as a string.
    compress: Whether to enable compression. Default is taken from ZipFile
        constructor.
    timestamp: The last modification date and time for the archive member.
  """
  assert (src_path is None) != (data is None), (
      '|src_path| and |data| are mutually exclusive.')
  if isinstance(zip_path, zipfile.ZipInfo):
    zipinfo = zip_path
    zip_path = zipinfo.filename
  else:
    zipinfo = zipfile.ZipInfo(filename=zip_path)
    zipinfo.external_attr = 0o644 << 16

  zipinfo.date_time = _hermetic_date_time(timestamp)

  # Filenames can contain backslashes, but it is more likely that we've
  # forgotten to use forward slashes as a directory separator.
  assert '\\' not in zip_path, 'zip_path should not contain \\: ' + zip_path
  assert not posixpath.isabs(zip_path), 'Absolute zip path: ' + zip_path
  assert not zip_path.startswith('..'), 'Should not start with ..: ' + zip_path
  assert posixpath.normpath(zip_path) == zip_path, (
      f'Non-canonical zip_path: {zip_path} vs: {posixpath.normpath(zip_path)}')
  assert zip_path not in zip_file.namelist(), (
      'Tried to add a duplicate zip entry: ' + zip_path)

  if src_path:
    with open(src_path, 'rb') as f:
      data = f.read()

  # zipfile will deflate even when it makes the file bigger. To avoid
  # growing files, disable compression at an arbitrary cut off point.
  if len(data) < 16:
    compress = False

  # None converts to ZIP_STORED, when passed explicitly rather than the
  # default passed to the ZipFile constructor.
  compress_type = zip_file.compression
  if compress is not None:
    compress_type = zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED
  zip_file.writestr(zipinfo, data, compress_type)


def add_files_to_zip(inputs,
                     output,
                     *,
                     base_dir=None,
                     path_transform=None,
                     compress=None,
                     zip_prefix_path=None,
                     timestamp=None):
  """Creates a zip file from a list of files.

  Args:
    inputs: A list of paths to zip, or a list of (zip_path, fs_path) tuples.
    output: Path, fileobj, or ZipFile instance to add files to.
    base_dir: Prefix to strip from inputs.
    path_transform: Called for each entry path. Returns a new zip path, or None
        to skip the file.
    compress: Whether to compress
    zip_prefix_path: Path prepended to file path in zip file.
    timestamp: Unix timestamp to use for files in the archive.
  """
  if base_dir is None:
    base_dir = '.'
  input_tuples = []
  for tup in inputs:
    if isinstance(tup, str):
      src_path = tup
      zip_path = os.path.relpath(src_path, base_dir)
      # Zip files always use / as path separator.
      if os.path.sep != posixpath.sep:
        zip_path = str(pathlib.Path(zip_path).as_posix())
      tup = (zip_path, src_path)
    input_tuples.append(tup)

  # Sort by zip path to ensure stable zip ordering.
  input_tuples.sort(key=lambda tup: tup[0])

  out_zip = output
  if not isinstance(output, zipfile.ZipFile):
    out_zip = zipfile.ZipFile(output, 'w')

  try:
    for zip_path, fs_path in input_tuples:
      if zip_prefix_path:
        zip_path = posixpath.join(zip_prefix_path, zip_path)
      if path_transform:
        zip_path = path_transform(zip_path)
        if zip_path is None:
          continue
      add_to_zip_hermetic(out_zip,
                          zip_path,
                          src_path=fs_path,
                          compress=compress,
                          timestamp=timestamp)
  finally:
    if output is not out_zip:
      out_zip.close()
