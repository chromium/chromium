# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common logic needed by other modules."""

import contextlib
import filecmp
import os
import shutil
import tempfile
import pathlib
import zipfile


def capitalize(value):
  return value[0].upper() + value[1:]


def escape_class_name(fully_qualified_class):
  """Returns an escaped string concatenating the Java package and class."""
  escaped = fully_qualified_class.replace('_', '_1')
  return escaped.replace('/', '_').replace('$', '_00024')


@contextlib.contextmanager
def atomic_output(path, mode='w+b'):
  with tempfile.NamedTemporaryFile(mode, delete=False) as f:
    try:
      yield f
    finally:
      f.close()

    if not (os.path.exists(path) and filecmp.cmp(f.name, path)):
      pathlib.Path(path).parents[0].mkdir(parents=True, exist_ok=True)
      shutil.move(f.name, path)
    if os.path.exists(f.name):
      os.unlink(f.name)


def add_to_zip_hermetic(zip_file, zip_path, data=None):
  zipinfo = zipfile.ZipInfo(filename=zip_path)
  zipinfo.external_attr = 0o644 << 16
  zipinfo.date_time = (2001, 1, 1, 0, 0, 0)
  zip_file.writestr(zipinfo, data, zipfile.ZIP_STORED)
