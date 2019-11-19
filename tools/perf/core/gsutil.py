# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import shutil
import subprocess
import tempfile


GSUTIL_BIN = 'gsutil'


def Copy(source_path, dest_path):
  subprocess.check_call([GSUTIL_BIN, 'cp', source_path, dest_path])


@contextlib.contextmanager
def OpenWrite(cloudpath):
  """Allows to "open" a cloud storage path for writing.

  Works by opening a local temporary file for writing, then copying the file
  to cloud storage when writing is done.
  """
  tempdir = tempfile.mkdtemp()
  try:
    localpath = os.path.join(tempdir, os.path.basename(cloudpath))
    with open(localpath, 'w') as f:
      yield f
    Copy(localpath, cloudpath)
  finally:
    shutil.rmtree(tempdir)
