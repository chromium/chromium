# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides gzip utilities for strings.
"""


import gzip
import io
import subprocess


def GzipString(data):
  # Gzipping using Python's built in gzip: Windows doesn't ship with gzip, and
  # OSX's gzip does not have an --rsyncable option built in. Although this is
  # not preferable to --rsyncable, it is an option for the systems that do
  # not have --rsyncable. If used over GzipStringRsyncable, the primary
  # difference of this function's compression will be larger updates every time
  # a compressed resource is changed.
  if isinstance(data, str):
    data = data.encode('utf8')
  gzip_output = io.BytesIO()
  with gzip.GzipFile(filename='', mode='wb', compresslevel=9,
                     fileobj=gzip_output, mtime=0) as gzip_file:
    gzip_file.write(data)
  data = gzip_output.getvalue()
  gzip_output.close()
  return data
