#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Retrieves an image's "fingerprint".

This is used when retrieving the image from the symbol server.  The .dll (or cab
compressed .dl_) or .exe is expected at a path like:
  foo.dll/FINGERPRINT/foo.dll
"""

from __future__ import print_function

import sys
import pefile


def GetImgFingerprint(filename):
  """Returns the fingerprint for an image file"""
  pe = pefile.PE(filename)
  return "%08X%x" % (
    pe.FILE_HEADER.TimeDateStamp, pe.OPTIONAL_HEADER.SizeOfImage)


def main():
  if len(sys.argv) != 2:
    print("usage: file.dll")
    return 1

  print(GetImgFingerprint(sys.argv[1]))
  return 0


if __name__ == '__main__':
  sys.exit(main())
