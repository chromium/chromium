# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides gzip utilities for strings.
"""

from __future__ import print_function

import gzip
import io
import subprocess


def GzipStringRsyncable(data):
  # Make call to host system's gzip to get access to --rsyncable option. This
  # option makes updates much smaller - if one line is changed in the resource,
  # it won't have to push the entire compressed resource with the update.
  # Instead, --rsyncable breaks the file into small chunks, so that one doesn't
  # affect the other in compression, and then only that chunk will have to be
  # updated.
  gzip_proc = subprocess.Popen(['gzip', '--stdout', '--rsyncable',
                                '--best', '--no-name'],
                               stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
  data, stderr = gzip_proc.communicate(data)
  if gzip_proc.returncode != 0:
    raise subprocess.CalledProcessError(gzip_proc.returncode, 'gzip',
                                        stderr)
  return data


def GzipString(data):
  # Gzipping using Python's built in gzip: Windows doesn't ship with gzip, and
  # OSX's gzip does not have an --rsyncable option built in. Although this is
  # not preferable to --rsyncable, it is an option for the systems that do
  # not have --rsyncable. If used over GzipStringRsyncable, the primary
  # difference of this function's compression will be larger updates every time
  # a compressed resource is changed.
  gzip_output = io.BytesIO()
  with gzip.GzipFile(mode='wb', compresslevel=9, fileobj=gzip_output,
                     mtime=0) as gzip_file:
    gzip_file.write(data)
  data = gzip_output.getvalue()
  gzip_output.close()
  return data
