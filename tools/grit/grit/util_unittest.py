#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test that checks some of util functions.
'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import unittest

from grit import util


class UtilUnittest(unittest.TestCase):
  ''' Tests functions from util
  '''

  def testNewClassInstance(self):
    # Test short class name with no fully qualified package name
    # Should fail, it is not supported by the function now (as documented)
    cls = util.NewClassInstance('grit.util.TestClassToLoad',
                                TestBaseClassToLoad)
    self.assertTrue(cls == None)

    # Test non existent class name
    cls = util.NewClassInstance('grit.util_unittest.NotExistingClass',
                                TestBaseClassToLoad)
    self.assertTrue(cls == None)

    # Test valid class name and valid base class
    cls = util.NewClassInstance('grit.util_unittest.TestClassToLoad',
                                TestBaseClassToLoad)
    self.assertTrue(isinstance(cls, TestBaseClassToLoad))

    # Test valid class name with wrong hierarchy
    cls = util.NewClassInstance('grit.util_unittest.TestClassNoBase',
                                TestBaseClassToLoad)
    self.assertTrue(cls == None)

  def testCanonicalLanguage(self):
    self.assertTrue(util.CanonicalLanguage('en') == 'en')
    self.assertTrue(util.CanonicalLanguage('pt_br') == 'pt-BR')
    self.assertTrue(util.CanonicalLanguage('pt-br') == 'pt-BR')
    self.assertTrue(util.CanonicalLanguage('pt-BR') == 'pt-BR')
    self.assertTrue(util.CanonicalLanguage('pt/br') == 'pt-BR')
    self.assertTrue(util.CanonicalLanguage('pt/BR') == 'pt-BR')
    self.assertTrue(util.CanonicalLanguage('no_no_bokmal') == 'no-NO-BOKMAL')

  def testUnescapeHtml(self):
    self.assertTrue(util.UnescapeHtml('&#1010;') == chr(1010))
    self.assertTrue(util.UnescapeHtml('&#xABcd;') == chr(43981))

  def testRelativePath(self):
    """ Verify that MakeRelativePath works in some tricky cases."""

    def TestRelativePathCombinations(base_path, other_path, expected_result):
      """ Verify that the relative path function works for
      the given paths regardless of whether or not they end with
      a trailing slash."""
      for path1 in [base_path, base_path + os.path.sep]:
        for path2 in [other_path, other_path + os.path.sep]:
          result = util.MakeRelativePath(path1, path2)
          self.assertTrue(result == expected_result)

    # set-up variables
    root_dir = 'c:%sa' % os.path.sep
    result1 = '..%sabc' % os.path.sep
    path1 = root_dir + 'bc'
    result2 = 'bc'
    path2 = '%s%s%s' % (root_dir, os.path.sep, result2)
    # run the tests
    TestRelativePathCombinations(root_dir, path1, result1)
    TestRelativePathCombinations(root_dir, path2, result2)

  def testReadFile(self):
    def Test(data, encoding, expected_result):
      with open('testfile', 'wb') as f:
        f.write(data)
      self.assertEqual(util.ReadFile('testfile', encoding), expected_result)

    test_std_newline = b'\xEF\xBB\xBFabc\ndef'  # EF BB BF is UTF-8 BOM
    newlines = [b'\n', b'\r\n', b'\r']

    with util.TempDir({}) as tmp_dir:
      with tmp_dir.AsCurrentDir():
        for newline in newlines:
          test = test_std_newline.replace(b'\n', newline)
          Test(test, util.BINARY, test)
          # utf-8 doesn't strip BOM
          Test(test, 'utf-8', test_std_newline.decode('utf-8'))
          # utf-8-sig strips BOM
          Test(test, 'utf-8-sig', test_std_newline.decode('utf-8')[1:])
          # test another encoding
          Test(test, 'cp1252', test_std_newline.decode('cp1252'))
        self.assertRaises(UnicodeDecodeError, Test, b'\x80', 'utf-8', None)


class TestBaseClassToLoad:
  pass

class TestClassToLoad(TestBaseClassToLoad):
  pass

class TestClassNoBase:
  pass


if __name__ == '__main__':
  unittest.main()
