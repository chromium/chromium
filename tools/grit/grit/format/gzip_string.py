# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides gzip utilities for strings.
"""

import gzip
import io


def GzipString(data):
  # Gzipping using Python's built in gzip. On Linux, previously there was
  # alternative that calls system gzip with --rsyncable to reduce delta.
  # However, this is of dubious value, and creates inconsistencies across. So
  # now only Python's gzip is used.
  if isinstance(data, str):
    data = data.encode('utf8')
  gzip_output = io.BytesIO()
  with gzip.GzipFile(filename='', mode='wb', compresslevel=9,
                     fileobj=gzip_output, mtime=0) as gzip_file:
    gzip_file.write(data)
  data = gzip_output.getvalue()
  gzip_output.close()
  return data
