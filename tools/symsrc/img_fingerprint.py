#!/usr/bin/env python3
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Retrieves an image's "fingerprint".

This is used when retrieving the image from the symbol server.  The .dll (or cab
compressed .dl_) or .exe is expected at a path like:
  foo.dll/FINGERPRINT/foo.dll
"""

import os
import sys

# Assume this script is under tools/symsrc/
_SCRIPT_DIR = os.path.dirname(__file__)
_ROOT_DIR = os.path.join(_SCRIPT_DIR, os.pardir, os.pardir)
_PEFILE_DIR = os.path.join(_ROOT_DIR, 'third_party', 'pefile_py3')

sys.path.insert(1, _PEFILE_DIR)

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
