# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bogus fingerprint implementation, do not use for production,
provided only as an example.

Usage:
    grit.py -h grit.extern.BogoFP xmb /tmp/foo
"""


import grit.extern.FP


def UnsignedFingerPrint(str, encoding='utf-8'):
  """Generate a fingerprint not intended for production from str (it
  reduces the precision of the production fingerprint by one bit).
  """
  return (0xFFFFF7FFFFFFFFFF &
          grit.extern.FP._UnsignedFingerPrintImpl(str, encoding))
