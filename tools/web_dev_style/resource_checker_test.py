#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import re
import resource_checker
from sys import path as sys_path
import test_util
import unittest

_HERE = os_path.dirname(os_path.abspath(__file__))
sys_path.append(os_path.join(_HERE, '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi


class ResourceCheckerTest(unittest.TestCase):
  def setUp(self):
    super(ResourceCheckerTest, self).setUp()

    self.checker = resource_checker.ResourceChecker(MockInputApi(),
                                                    MockOutputApi())

  def ShouldPassDeprecatedMojoBindingCheck(self, line):
    error = self.checker.DeprecatedMojoBindingsCheck(1, line)
    self.assertEqual('', error, 'Should not be flagged as error: ' + line)

  def ShouldFailDeprecatedMojoBindingCheck(self, line):
    error = self.checker.DeprecatedMojoBindingsCheck(1, line)
    self.assertNotEqual('', error, 'Should be flagged as error: ' + line)
    self.assertEquals('mojo_bindings.js', test_util.GetHighlight(line, error))

  def testDeprecatedMojoBindingsCheckPasses(self):
    lines = [
      '<script src="chrome://resources/js/mojo_bindings_lite.js">',
      "script.src = 'chrome://resources/js/mojo_bindings_lite.js';",
    ]
    for line in lines:
      self.ShouldPassDeprecatedMojoBindingCheck(line)

  def testDeprecatedMojoBindingsCheckFails(self):
    lines = [
      '<script src="chrome://resources/js/mojo_bindings.js">',
      "script.src = 'chrome://resources/js/mojo_bindings.js';",
    ]
    for line in lines:
      self.ShouldFailDeprecatedMojoBindingCheck(line)

  def ShouldPassDisallowIncludeCheck(self, line):
    self.assertEqual('', self.checker.DisallowIncludeCheck('msg', 1, line),
                     'Should not be flagged as error')

  def ShouldFailDisallowIncludeCheck(self, line):
    error = self.checker.DisallowIncludeCheck('msg', 1, line)
    self.assertNotEqual('', error, 'Should be flagged as error: ' + line)
    self.assertEquals('<include', test_util.GetHighlight(line, error))

  def testDisallowIncludesFails(self):
    lines = [
      '<include src="blah.js">',
      ' // <include src="blah.js">',
      '  /* <include  src="blah.js"> */ ',
    ]
    for line in lines:
      self.ShouldFailDisallowIncludeCheck(line)

  def testDisallowIncludesPasses(self):
    lines = [
      'if (count < includeCount) {',
      '// No <include>s allowed.',
    ]
    for line in lines:
      self.ShouldPassDisallowIncludeCheck(line)

  def ShouldFailSelfClosingIncludeCheck(self, line):
    """Checks that the '</include>' checker flags |line| as a style error."""
    error = self.checker.SelfClosingIncludeCheck(1, line)
    self.assertNotEqual('', error,
        'Should be flagged as style error: ' + line)
    highlight = test_util.GetHighlight(line, error).strip()
    self.assertTrue('include' in highlight and highlight[0] == '<')

  def ShouldPassSelfClosingIncludeCheck(self, line):
    """Checks that the '</include>' checker doesn't flag |line| as an error."""
    self.assertEqual('', self.checker.SelfClosingIncludeCheck(1, line),
        'Should not be flagged as style error: ' + line)

  def testIncludeFails(self):
    lines = [
        "</include>   ",
        "    </include>",
        "    </include>   ",
        '  <include src="blah.js" />   ',
        '<include src="blee.js"/>',
    ]
    for line in lines:
      self.ShouldFailSelfClosingIncludeCheck(line)

  def testIncludePasses(self):
    lines = [
        '<include src="assert.js">',
        "<include src='../../assert.js'>",
        "<i>include src='blah'</i>",
        "</i>nclude",
        "</i>include",
    ]
    for line in lines:
      self.ShouldPassSelfClosingIncludeCheck(line)


if __name__ == '__main__':
  unittest.main()
