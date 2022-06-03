# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Utility classes for testing goma_link.

import contextlib
import os
import shutil
import tempfile


# tempfile.NamedDirectory is in Python 3.8. This is for compatibility with
# older Python versions.
@contextlib.contextmanager
def named_directory(*args, **kwargs):
  name = tempfile.mkdtemp(*args, **kwargs)
  try:
    yield name
  finally:
    shutil.rmtree(name)


@contextlib.contextmanager
def working_directory(newcwd):
  """
  Changes working directory to the specified directory, runs enclosed code,
  and changes back to the previous directory.
  """
  oldcwd = os.getcwd()
  os.chdir(newcwd)
  try:
    # Use os.getcwd() instead of newcwd so that we have a path that works
    # inside the block.
    yield os.getcwd()
  finally:
    os.chdir(oldcwd)
