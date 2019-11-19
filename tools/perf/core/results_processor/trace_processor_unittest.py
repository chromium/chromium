# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import os
import shutil
import tempfile
import unittest

from core.results_processor import trace_processor

import mock


class TraceProcessorTestCase(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def testConvertProtoTracesToJson(self):
    trace_plain = os.path.join(self.temp_dir, 'trace1.pb')
    with open(trace_plain, 'w') as f:
      f.write('a')
    trace_gzipped = os.path.join(self.temp_dir, 'trace2.pb.gz')
    with gzip.open(trace_gzipped, 'w') as f:
      f.write('b')

    with mock.patch('os.path.isfile', return_value=True):
      with mock.patch('subprocess.check_call'):
        trace_processor.ConvertProtoTracesToJson(
            '/path/to/tp', [trace_plain, trace_gzipped], '/path/to/json')
