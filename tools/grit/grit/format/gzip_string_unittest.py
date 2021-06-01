#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.gzip_string'''

from __future__ import print_function

import gzip
import io
import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit.format import gzip_string


class FormatGzipStringUnittest(unittest.TestCase):

  def testGzipStringRsyncable(self):
    # Can only test the rsyncable version on platforms which support rsyncable,
    # which at the moment is Linux.
    if sys.platform == 'linux2':
      header_begin = (b'\x1f\x8b')  # gzip first two bytes
      input = (b'TEST STRING STARTING NOW'
               b'continuing'
               b'<even more>'
               b'<finished NOW>')

      compressed = gzip_string.GzipStringRsyncable(input)
      self.failUnless(header_begin == compressed[:2])

      compressed_file = io.BytesIO()
      compressed_file.write(compressed)
      compressed_file.seek(0)

      with gzip.GzipFile(mode='rb', fileobj=compressed_file) as f:
        output = f.read()
      self.failUnless(output == input)

  def testGzipString(self):
    header_begin = b'\x1f\x8b'  # gzip first two bytes
    input = (b'TEST STRING STARTING NOW'
             b'continuing'
             b'<even more>'
             b'<finished NOW>')

    compressed = gzip_string.GzipString(input)
    self.failUnless(header_begin == compressed[:2])

    compressed_file = io.BytesIO()
    compressed_file.write(compressed)
    compressed_file.seek(0)

    with gzip.GzipFile(mode='rb', fileobj=compressed_file) as f:
      output = f.read()
    self.failUnless(output == input)


if __name__ == '__main__':
  unittest.main()
