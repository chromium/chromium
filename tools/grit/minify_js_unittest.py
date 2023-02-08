#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import minify_js


class MinifyWithUglifyTest(unittest.TestCase):
  def test_simple(self):
    source = """
            var foo = 0;
        """
    minimized = minify_js.Minify(source)
    self.assertEqual(minimized, "var foo=0;")

  def test_complex(self):
    source = """
            // comments should be removed
            var foo = {
                "bar": 0,
                baz: 5,
            };
            var qux = foo.bar + foo.baz;
        """
    minimized = minify_js.Minify(source)
    self.assertEqual(minimized,
                     "var foo={bar:0,baz:5};var qux=foo.bar+foo.baz;")
