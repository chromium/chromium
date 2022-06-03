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


"""Unit test for depswriter."""

__author__ = 'johnlenz@google.com (John Lenz)'


import unittest

import depswriter


class MockSource(object):
  """Mock Source file."""

  def __init__(self, provides, requires, is_goog_module=False):
    self.provides = set(provides)
    self.requires = set(requires)
    self.is_goog_module = is_goog_module

  def __repr__(self):
    return 'MockSource %s' % self.provides


class DepsWriterTestCase(unittest.TestCase):
  """Unit test for depswriter."""

  def testMakeDepsFile(self):
    sources = {}
    sources['test.js'] = MockSource(['A'], ['B', 'C'])
    deps = depswriter.MakeDepsFile(sources)

    self.assertEqual(
        'goog.addDependency(\'test.js\', [\'A\'], [\'B\', \'C\'], {});\n',
        deps)

  def testMakeDepsFileUnicode(self):
    sources = {}
    sources['test.js'] = MockSource([u'A'], [u'B', u'C'])
    deps = depswriter.MakeDepsFile(sources)

    self.assertEqual(
        'goog.addDependency(\'test.js\', [\'A\'], [\'B\', \'C\'], {});\n',
        deps)

  def testMakeDepsFileModule(self):
    sources = {}
    sources['test.js'] = MockSource(['A'], ['B', 'C'], True)
    deps = depswriter.MakeDepsFile(sources)

    self.assertEqual(
        "goog.addDependency('test.js', "
        "['A'], ['B', 'C'], {'module': 'goog'});\n",
        deps)

if __name__ == '__main__':
  unittest.main()
