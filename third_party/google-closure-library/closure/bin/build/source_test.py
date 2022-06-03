#!/usr/bin/env python
#
# Copyright 2010 The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""Unit test for source."""

__author__ = 'nnaze@google.com (Nathan Naze)'


import unittest

import source


class SourceTestCase(unittest.TestCase):
  """Unit test for source.  Tests the parser on a known source input."""

  def testSourceScan(self):
    test_source = source.Source(_TEST_SOURCE)

    self.assertEqual(set(['foo', 'foo.test']),
                     test_source.provides)
    self.assertEqual(set(['goog.dom', 'goog.events.EventType']),
                     test_source.requires)
    self.assertFalse(test_source.is_goog_module)

  def testSourceScanBase(self):
    test_source = source.Source(_TEST_BASE_SOURCE)

    self.assertEqual(set(['goog']),
                     test_source.provides)
    self.assertEqual(test_source.requires, set())
    self.assertFalse(test_source.is_goog_module)

  def testSourceScanBadBase(self):

    def MakeSource():
      source.Source(_TEST_BAD_BASE_SOURCE)

    self.assertRaises(Exception, MakeSource)

  def testSourceScanGoogModule(self):
    test_source = source.Source(_TEST_MODULE_SOURCE)

    self.assertEqual(set(['foo']),
                     test_source.provides)
    self.assertEqual(set(['bar']),
                     test_source.requires)
    self.assertTrue(test_source.is_goog_module)

  def testSourceScanModuleAlias(self):
    test_source = source.Source(_TEST_MODULE_ALIAS_SOURCE)

    self.assertEqual(set(['goog.dom', 'goog.events']), test_source.requires)
    self.assertTrue(test_source.is_goog_module)

  def testStripComments(self):
    self.assertEqual(
        '\nvar foo = function() {}',
        source.Source._StripComments(('/* This is\n'
                                      '  a comment split\n'
                                      '  over multiple lines\n'
                                      '*/\n'
                                      'var foo = function() {}')))

  def testGoogStatementsInComments(self):
    test_source = source.Source(_TEST_COMMENT_SOURCE)

    self.assertEqual(set(['foo']),
                     test_source.provides)
    self.assertEqual(set(['goog.events.EventType']),
                     test_source.requires)
    self.assertFalse(test_source.is_goog_module)

  def testHasProvideGoog(self):
    self.assertTrue(source.Source._HasProvideGoogFlag(_TEST_BASE_SOURCE))
    self.assertTrue(source.Source._HasProvideGoogFlag(_TEST_BAD_BASE_SOURCE))
    self.assertFalse(source.Source._HasProvideGoogFlag(_TEST_COMMENT_SOURCE))


_TEST_MODULE_ALIAS_SOURCE = """
goog.module('foo');
const {createDom: domCreator} = goog.require('goog.dom');
const {listen} = goog.require('goog.events');
"""


_TEST_MODULE_SOURCE = """
goog.module('foo');
var b = goog.require('bar');
"""


_TEST_SOURCE = """// Fake copyright notice

/** Very important comment. */

goog.provide('foo');
goog.provide('foo.test');

goog.require('goog.dom');
goog.require('goog.events.EventType');

function foo() {
  // Set bar to seventeen to increase performance.
  this.bar = 17;
}
"""

_TEST_COMMENT_SOURCE = """// Fake copyright notice

goog.provide('foo');

/*
goog.provide('foo.test');
 */

/*
goog.require('goog.dom');
*/

// goog.require('goog.dom');

goog.require('goog.events.EventType');

function bar() {
  this.baz = 55;
}
"""

_TEST_BASE_SOURCE = """
/**
 * @fileoverview The base file.
 * @provideGoog
 */

var goog = goog || {};
"""

_TEST_BAD_BASE_SOURCE = """
/**
 * @fileoverview The base file.
 * @provideGoog
 */

goog.provide('goog');
"""


if __name__ == '__main__':
  unittest.main()
