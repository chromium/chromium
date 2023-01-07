# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import customtabs_benchmark


class CustomTabsBenchmarkTestCase(unittest.TestCase):
  def testParseResult(self):
    result_line = (
        "1,0,disabled,1000,3000,510998167,510998345,511000338,510999329")
    result = customtabs_benchmark.ParseResult(result_line)
    self.assertEquals(1, result.warmup)
    self.assertEquals(0, result.skip_launcher_activity)
    self.assertEquals('disabled', result.speculation_mode)
    self.assertEquals(1000, result.delay_to_may_launch_url)
    self.assertEquals(3000, result.delay_to_launch_url)
    self.assertEquals(510998345 - 510998167, result.commit)
    self.assertEquals(511000338 - 510998167, result.plt)
    self.assertEquals(510999329 - 510998167, result.first_contentful_paint)

  def testParsePartialResult(self):
    result_line = (
        "1,0,disabled,1000,3000,510998167,-1,-1,510999329")
    result = customtabs_benchmark.ParseResult(result_line)
    self.assertEquals(1, result.warmup)
    self.assertEquals(0, result.skip_launcher_activity)
    self.assertEquals('disabled', result.speculation_mode)
    self.assertEquals(1000, result.delay_to_may_launch_url)
    self.assertEquals(3000, result.delay_to_launch_url)
    self.assertEquals(-1, result.commit)
    self.assertEquals(-1, result.plt)
    self.assertEquals(510999329 - 510998167, result.first_contentful_paint)


if __name__ == '__main__':
  unittest.main()
