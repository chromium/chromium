#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from cpp_util import (Classname, CloseNamespace, GetCppNamespace,
                      GenerateIfndefName, OpenNamespace)


class CppUtilTest(unittest.TestCase):

  def testClassname(self):
    self.assertEqual('Permissions', Classname('permissions'))
    self.assertEqual('UpdateAllTheThings', Classname('updateAllTheThings'))
    self.assertEqual('Aa_Bb_Cc', Classname('aa.bb.cc'))

  def testNamespaceDeclaration(self):
    self.assertEqual('namespace foo {', OpenNamespace('foo').Render())
    self.assertEqual('}  // namespace foo', CloseNamespace('foo').Render())

    self.assertEqual('namespace extensions {\n'
                     'namespace foo {',
                     OpenNamespace('extensions::foo').Render())
    self.assertEqual('}  // namespace foo\n'
                     '}  // namespace extensions',
                     CloseNamespace('extensions::foo').Render())

    self.assertEqual(
        'namespace extensions {\n'
        'namespace gen {\n'
        'namespace api {',
        OpenNamespace('extensions::gen::api').Render())
    self.assertEqual(
        '}  // namespace api\n'
        '}  // namespace gen\n'
        '}  // namespace extensions',
        CloseNamespace('extensions::gen::api').Render())

    self.assertEqual(
        'namespace extensions {\n'
        'namespace gen {\n'
        'namespace api {\n'
        'namespace foo {',
        OpenNamespace('extensions::gen::api::foo').Render())
    self.assertEqual(
        '}  // namespace foo\n'
        '}  // namespace api\n'
        '}  // namespace gen\n'
        '}  // namespace extensions',
        CloseNamespace('extensions::gen::api::foo').Render())

  def testGenerateIfndefName(self):
    self.assertEqual('FOO_BAR_BAZ_H__', GenerateIfndefName('foo\\bar\\baz.h'))
    self.assertEqual('FOO_BAR_BAZ_H__', GenerateIfndefName('foo/bar/baz.h'))

  def testGetCppNamespace(self):
    namespace_pattern = "test::api::%(namespace)s"
    unix_name = "SimpleApi"
    self.assertEqual("test::api::SimpleApi",
                     GetCppNamespace(namespace_pattern, unix_name))


if __name__ == '__main__':
  unittest.main()
