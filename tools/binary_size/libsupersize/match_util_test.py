#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

import match_util


class MatchHelperTest(unittest.TestCase):

  def testExpandRegexIdentifierPlaceholder(self):
    def matches(pattern, target):
      regex = re.compile(match_util.ExpandRegexIdentifierPlaceholder(pattern))
      return bool(regex.search(target) and regex.search(' %s ' % target))

    self.assertTrue(matches(r'{{hello_world}}', 'helloWorld'))
    self.assertTrue(matches(r'{{hello_world}}', 'HelloWorld'))
    self.assertTrue(matches(r'{{hello_world}}', 'kHelloWorld'))
    self.assertTrue(matches(r'{{hello_world}}', 'hello_world'))
    self.assertTrue(matches(r'{{hello_world}}', '_hello_world'))
    self.assertTrue(matches(r'{{hello_world}}', 'hello_world_}}'))
    self.assertTrue(matches(r'{{hello_world}}', 'HELLO_WORLD'))
    self.assertTrue(matches(r'{{hello_world}}', 'HELLO_WORLD1'))
    self.assertTrue(matches(r'{{hello_world}}', 'kHelloWorld1'))
    self.assertFalse(matches(r'{{hello_world}}', 'hello world'))
    self.assertFalse(matches(r'{{hello_world}}', 'helloworld'))
    self.assertFalse(matches(r'{{hello_world}}', 'f_hello_world'))
    self.assertFalse(matches(r'{{hello_world}}', 'hello_world_f'))
    self.assertFalse(matches(r'{{hello_world}}', 'hello/world'))
    self.assertFalse(matches(r'{{hello_world}}', 'helloWORLD'))
    self.assertFalse(matches(r'{{hello_world}}', 'HELLOworld'))
    self.assertFalse(matches(r'{{hello_world}}', 'HELLOWORLD'))
    self.assertFalse(matches(r'{{hello_world}}', 'FOO_HELLO_WORLD'))
    self.assertFalse(matches(r'{{hello_world}}', 'HELLO_WORLD_BAR'))
    self.assertFalse(matches(r'{{hello_world}}', 'FooHelloWorld'))
    self.assertFalse(matches(r'{{hello_world}}', 'HelloWorldBar'))
    self.assertFalse(matches(r'{{hello_world}}', 'foohello/world'))
    self.assertFalse(matches(r'{{hello_world}}', '1HELLO_WORLD'))

    self.assertTrue(matches(r'{{_hello_world_}}', 'helloWorld'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'HelloWorld'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'kHelloWorld'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'hello_world'))
    self.assertTrue(matches(r'{{_hello_world_}}', '_hello_world'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'hello_world_'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'HELLO_WORLD'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'HELLO_WORLD1'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'kHelloWorld1'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'hello world'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'helloworld'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'f_hello_world'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'hello_world_f'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'hello/world'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'helloWORLD'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'HELLOworld'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'HELLOWORLD'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'FOO_HELLO_WORLD'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'HELLO_WORLD_BAR'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'FooHelloWorld'))
    self.assertTrue(matches(r'{{_hello_world_}}', 'HelloWorldBar'))
    self.assertFalse(matches(r'{{_hello_world_}}', 'foohello/world'))
    self.assertFalse(matches(r'{{_hello_world_}}', '1HELLO_WORLD'))


if __name__ == '__main__':
  unittest.main()
