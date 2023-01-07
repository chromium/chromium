# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import blink_perf


# pylint: disable=protected-access
class BlinkPerfXMLHttpRequest(blink_perf._BlinkPerfBenchmark):
  SUBDIR = 'xml_http_request'
