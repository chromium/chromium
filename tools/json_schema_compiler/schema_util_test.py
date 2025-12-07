#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from schema_util import JsFunctionNameToClassName
from schema_util import StripNamespace
import unittest


class SchemaUtilTest(unittest.TestCase):

  def testStripNamespace(self):
    self.assertEqual('Bar', StripNamespace('foo.Bar'))
    self.assertEqual('Baz', StripNamespace('Baz'))

  def testJsFunctionNameToClassName(self):
    self.assertEqual('FooBar', JsFunctionNameToClassName('foo', 'bar'))
    self.assertEqual('FooBar',
                     JsFunctionNameToClassName('experimental.foo', 'bar'))
    self.assertEqual('FooBarBaz', JsFunctionNameToClassName('foo.bar', 'baz'))
    self.assertEqual('FooBarBaz',
                     JsFunctionNameToClassName('experimental.foo.bar', 'baz'))


if __name__ == '__main__':
  unittest.main()
