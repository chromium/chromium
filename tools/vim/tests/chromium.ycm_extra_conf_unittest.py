#!/usr/bin/env python3

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for chromium.ycm_extra_conf.

These tests should be getting picked up by the PRESUBMIT.py in /tools/vim.
Currently the tests only run on Linux and require 'ninja' to be available on
PATH. Due to these requirements, the tests should only be run on upload.
"""

import imp
import os
import shutil
import stat
import string
import subprocess
import sys
import tempfile
import unittest


def CreateFile(path, copy_from=None, format_with=None, make_executable=False):
  """Creates a file.

  If a file already exists at |path|, it will be overwritten.

  Args:
    path: (String) Absolute path for file to be created.
    copy_from: (String or None) Absolute path to source file. If valid, the
               contents of this file will be written to |path|.
    format_with: (Dictionary or None) Only valid if |copy_from| is also valid.
               The contents of the file at |copy_from| will be passed through
               string.Formatter.vformat() with this parameter as the dictionary.
    make_executable: (Boolean) If true, |file| will be made executable.
  """
  if not os.path.isabs(path):
    raise Exception(
        'Argument |path| needs to be an absolute path. Got: "{}"'.format(path))
  with open(path, 'w') as f:
    if copy_from:
      with open(copy_from, 'r') as source:
        contents = source.read()
        if format_with:
          formatter = string.Formatter()
          contents = formatter.vformat(contents, None, format_with)
        f.write(contents)
  if make_executable:
    statinfo = os.stat(path)
    os.chmod(path, statinfo.st_mode | stat.S_IXUSR)


def GetLastLangFlag(flags):
  lastLang = None
  for i, flag in enumerate(flags):
    if flag == '-x':
      lastLang = flags[i + 1]
  return lastLang


def TestLanguage(test_file, language):

  def test(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, test_file))
    self.assertTrue(result)
    self.assertEqual(GetLastLangFlag(result['flags']), language)

  return test


class Chromium_ycmExtraConfTest(unittest.TestCase):

  def SetUpFakeChromeTreeBelowPath(self):
    """Create fake Chromium source tree under self.test_root.

    The fake source tree has the following contents:

    <self.test_root>
      |  .gclient
      |
      +-- src
      |   |  DEPS
      |   |  three.cc
      |   |
      |   +-- .git
      |
      +-- out
          |
          +-- gn
                build.ninja
    """
    self.chrome_root = os.path.abspath(
        os.path.normpath(os.path.join(self.test_root, 'src')))
    self.out_dir = os.path.join(self.chrome_root, 'out', 'gn')

    os.makedirs(self.chrome_root)
    os.makedirs(os.path.join(self.chrome_root, '.git'))
    os.makedirs(self.out_dir)

    CreateFile(os.path.join(self.test_root, '.gclient'))
    CreateFile(os.path.join(self.chrome_root, 'DEPS'))
    CreateFile(os.path.join(self.chrome_root, 'three.cc'))

    # Fake ninja build file. Applications of 'cxx' rule are tagged by which
    # source file was used as input so that the test can verify that the correct
    # build dependency was used.
    CreateFile(
        os.path.join(self.out_dir, 'build.ninja'),
        copy_from=os.path.join(self.test_data_path, 'fake_build_ninja.txt'))

  def NormalizeString(self, string):
    return string.replace(self.out_dir, '[OUT]').\
        replace(self.chrome_root, '[SRC]').replace('\\', '/')

  def NormalizeStringsInList(self, list_of_strings):
    return [self.NormalizeString(s) for s in list_of_strings]

  def setUp(self):
    self.actual_chrome_root = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../..'))
    sys.path.append(os.path.join(self.actual_chrome_root, 'tools', 'vim'))
    self.test_data_path = os.path.join(self.actual_chrome_root, 'tools', 'vim',
                                       'tests', 'data')
    self.ycm_extra_conf = imp.load_source('ycm_extra_conf',
                                          'chromium.ycm_extra_conf.py')
    self.test_root = tempfile.mkdtemp()
    self.SetUpFakeChromeTreeBelowPath()

  def tearDown(self):
    if self.test_root:
      shutil.rmtree(self.test_root)

  def testNinjaIsAvailable(self):
    p = subprocess.Popen(['ninja', '--version'], stdout=subprocess.PIPE)
    _, _ = p.communicate()
    self.assertFalse(p.returncode)

  def testFindChromeSrc(self):
    chrome_source = self.ycm_extra_conf.FindChromeSrcFromFilename(
        os.path.join(self.chrome_root, 'chrome', 'one.cpp'))
    self.assertEqual(chrome_source, self.chrome_root)

    chrome_source = self.ycm_extra_conf.FindChromeSrcFromFilename(
        os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEqual(chrome_source, self.chrome_root)

  def testCommandLineForKnownCppFile(self):
    command_line = self.ycm_extra_conf.GetClangCommandLineFromNinjaForSource(
        self.out_dir, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEqual(command_line,
                     ('../../fake-clang++ -Ia -Itag-one ../../one.cpp '
                      '-o obj/one.o'))

  def testCommandLineForUnknownCppFile(self):
    command_line = self.ycm_extra_conf.GetClangCommandLineFromNinjaForSource(
        self.out_dir, os.path.join(self.chrome_root, 'unknown.cpp'))
    self.assertEqual(command_line, None)

  def testGetClangOptionsForKnownCppFile(self):
    clang_options = \
        self.ycm_extra_conf.GetClangOptionsFromNinjaForFilename(
            self.chrome_root, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEqual(self.NormalizeStringsInList(clang_options), [
        '-I[SRC]', '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-one'
    ])

  def testOutDirNames(self):
    out_root = os.path.join(self.chrome_root, 'out_with_underscore')
    out_dir = os.path.join(out_root, 'gn')
    shutil.move(os.path.join(self.chrome_root, 'out'), out_root)

    clang_options = \
        self.ycm_extra_conf.GetClangOptionsFromNinjaForFilename(
            self.chrome_root, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertIn('-I%s/a' % self.NormalizeString(out_dir),
                  self.NormalizeStringsInList(clang_options))
    self.assertIn('-I%s/tag-one' % self.NormalizeString(out_dir),
                  self.NormalizeStringsInList(clang_options))

  def testGetFlagsForFileForKnownCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'one.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-one'
    ])

  def testGetFlagsForFileForUnknownCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default'
    ])

  def testGetFlagsForFileForUnknownCppNotTestFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'test_nonexistent.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default'
    ])

  testGetFlagsForFileForKnownObjcFile = TestLanguage('eight.m', 'objective-c')
  testGetFlagsForFileForKnownObjcHeaderFile = TestLanguage(
      'eight.h', 'objective-c')
  testGetFlagsForFileForUnknownObjcFile = TestLanguage('nonexistent.m',
                                                       'objective-c')
  testGetFlagsForFileForKnownObjcppFile = TestLanguage('nine.mm',
                                                       'objective-c++')
  testGetFlagsForFileForKnownObjcppHeaderFile = TestLanguage(
      'nine.h', 'objective-c++')
  testGetFlagsForFileForUnknownObjcppFile = TestLanguage(
      'nonexistent.mm', 'objective-c++')

  def testGetFlagsForFileForUnknownHeaderFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent.h'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default'
    ])

  def testGetFlagsForFileForUnknownUnittestFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent_unittest.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default-test'
    ])

  def testGetFlagsForFileForUnknownBrowsertestFile2(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent_browsertest.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default-test'
    ])

  def testGetFlagsForFileForKnownHeaderFileWithAssociatedCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'three.h'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-three'
    ])

  def testSourceFileWithNonClangOutputs(self):
    # Verify assumption that four.cc has non-compiler-output listed as the first
    # output.
    p = subprocess.Popen(
        ['ninja', '-C', self.out_dir, '-t', 'query', '../../four.cc'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True)
    stdout, _ = p.communicate()
    self.assertFalse(p.returncode)
    self.assertEqual(
        stdout, '../../four.cc:\n'
        '  outputs:\n'
        '    obj/linker-output.o\n'
        '    obj/four.o\n')

    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'four.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-four'
    ])

  def testSourceFileWithOnlyNonClangOutputs(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'five.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/a', '-I[OUT]/tag-default'
    ])

  @unittest.skipIf(os.name == "nt", "Test fails with path differences on "
                   "Windows.")
  def testGetFlagsForSysrootAbsPath(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'six.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++14',
        '-x',
        'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '--sysroot=/usr/lib/sysroot-image',
        '-isysroot',
        '/mac.sdk',
    ])

  def testGetFlagsForSysrootRelPath(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'seven.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++14',
        '-x',
        'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '--sysroot=[SRC]/build/sysroot-image',
        '-isysroot',
        '[SRC]/build/mac.sdk',
    ])

  @unittest.skipIf(os.name == "nt", "Test fails with path differences on "
                   "Windows.")
  def testGetFlagsForIsystem(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'ten.cc'))
    self.assertTrue(result)
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I[OUT]/b', '-isystem[OUT]/a',
        '-isystem', '[SRC]/build/c', '-isystem', '/usr/lib/include'
    ])

  def testGetFlagsTwoPartI(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'eleven.cc'))
    self.assertTrue(result)
    self.assertTrue('flags' in result)
    self.assertEqual(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER', '-std=c++14', '-x', 'c++', '-I[SRC]',
        '-Wno-unknown-warning-option', '-I', '[OUT]/a', '-I', '[OUT]/tag-eleven'
    ])


if __name__ == '__main__':
  if not os.path.isfile('chromium.ycm_extra_conf.py'):
    print('The test must be run from src/tools/vim/ directory')
    sys.exit(1)
  unittest.main()
