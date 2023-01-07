#!/usr/bin/env python
#
# Copyright The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required `by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""Unit test for generate_jsdoc."""

__author__ = 'nnaze@google.com (Nathan Naze)'


import re
import unittest

import generate_jsdoc


class InsertJsDocTestCase(unittest.TestCase):
  """Unit test for source.  Tests the parser on a known source input."""

  def testMatchFirstFunction(self):
    match = generate_jsdoc._MatchFirstFunction(_TEST_SOURCE)
    self.assertNotEqual(None, match)
    self.assertEqual('aaa, bbb, ccc', match.group('arguments'))

    match = generate_jsdoc._MatchFirstFunction(_INDENTED_SOURCE)
    self.assertNotEqual(None, match)
    self.assertEqual('', match.group('arguments'))

    match = generate_jsdoc._MatchFirstFunction(_ODD_NEWLINES_SOURCE)
    self.assertEqual('goog.\nfoo.\nbar\n.baz.\nqux', match.group('identifier'))

  def testParseArgString(self):
    self.assertEqual(['foo', 'bar', 'baz'],
                     list(generate_jsdoc._ParseArgString('foo, bar, baz')))

  def testExtractFunctionBody(self):
    self.assertEqual('\n  // Function comments.\n  return;\n',
                     generate_jsdoc._ExtractFunctionBody(_TEST_SOURCE))

    self.assertEqual('\n    var bar = 3;\n    return true;\n',
                     generate_jsdoc._ExtractFunctionBody(_INDENTED_SOURCE, 2))

  def testContainsValueReturn(self):
    self.assertTrue(generate_jsdoc._ContainsReturnValue(_INDENTED_SOURCE))
    self.assertFalse(generate_jsdoc._ContainsReturnValue(_TEST_SOURCE))

  def testInsertString(self):
    self.assertEqual('abc123def',
                     generate_jsdoc._InsertString('abcdef', '123', 3))

  def testInsertJsDoc(self):
    self.assertEqual(_EXPECTED_INDENTED_SOURCE,
                     generate_jsdoc.InsertJsDoc(_INDENTED_SOURCE))

    self.assertEqual(_EXPECTED_TEST_SOURCE,
                     generate_jsdoc.InsertJsDoc(_TEST_SOURCE))

    self.assertEqual(_EXPECTED_ODD_NEWLINES_SOURCE,
                     generate_jsdoc.InsertJsDoc(_ODD_NEWLINES_SOURCE))


_INDENTED_SOURCE = """\
  boo.foo.woo = function() {
    var bar = 3;
    return true;
  };
"""

_EXPECTED_INDENTED_SOURCE = """\
  /**
   * @return
   */
  boo.foo.woo = function() {
    var bar = 3;
    return true;
  };
"""


_TEST_SOURCE = """\

//  Random comment.

goog.foo.bar = function (aaa, bbb, ccc) {
  // Function comments.
  return;
};
"""

_EXPECTED_TEST_SOURCE = """\

//  Random comment.

/**
 * @param {} aaa
 * @param {} bbb
 * @param {} ccc
 */
goog.foo.bar = function (aaa, bbb, ccc) {
  // Function comments.
  return;
};
"""

_ODD_NEWLINES_SOURCE = """\
goog.
foo.
bar
.baz.
qux
   =

 function

(aaa,

bbb, ccc) {
  // Function comments.
  return;
};
"""

_EXPECTED_ODD_NEWLINES_SOURCE = """\
/**
 * @param {} aaa
 * @param {} bbb
 * @param {} ccc
 */
goog.
foo.
bar
.baz.
qux
   =

 function

(aaa,

bbb, ccc) {
  // Function comments.
  return;
};
"""

if __name__ == '__main__':
  unittest.main()
