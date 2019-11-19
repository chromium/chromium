#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import minimize_css


class CSSMinimizerTest(unittest.TestCase):

    def test_simple(self):
        source = """
            div {
                color: blue;
            }
        """
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(minimized, "div{color: blue}")

    def test_attribute_selectors(self):
        source = """
            input[type="search" i]::-webkit-textfield-decoration-container {
                direction: ltr;
            }
        """
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(
            minimized,
            # pylint: disable=line-too-long
            """input[type="search" i]::-webkit-textfield-decoration-container{direction: ltr}""")

    def test_strip_comment(self):
        source = """
        /* header */
        html {
            /* inside block */
            display: block;
        }
        /* footer */
        """
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(minimized, "html{ display: block}")

    def test_no_strip_inside_quotes(self):
        source = """div[foo=' bar ']"""
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(minimized, source)

        source = """div[foo=" bar "]"""
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(minimized, source)

    def test_escape_string(self):
        source = """content: " <a onclick=\\\"javascript:  alert  ( 'foobar' ); \\\">";"""
        minimized = minimize_css.CSSMinimizer.minimize_css(source)
        self.assertEquals(minimized, source)
