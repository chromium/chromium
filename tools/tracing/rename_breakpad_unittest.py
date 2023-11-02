#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from logging import exception
import os
import sys
import unittest
import tempfile
import shutil

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()
import py_utils

import rename_breakpad
import mock


class RenameBreakpadFilesTestCase(unittest.TestCase):
  def setUp(self):
    self.breakpad_dir = tempfile.mkdtemp()
    self.breakpad_output_dir = tempfile.mkdtemp()

    # Directory to unzipped breakpad files.
    self.breakpad_unzip_dir = tempfile.mkdtemp(dir=self.breakpad_dir)

  def tearDown(self):
    shutil.rmtree(self.breakpad_dir)
    shutil.rmtree(self.breakpad_output_dir)

  def _listSubtree(self, root):
    """Returns absolute paths of files and dirs in root subtree.
    """
    files = set()
    dirs = set()
    for topdir, subdirs, filenames in os.walk(root):
      for filename in filenames:
        files.add(os.path.join(topdir, filename))
      for subdir in subdirs:
        dirs.add(os.path.join(topdir, subdir))
    return dirs, files

  def _assertFilesInInputDir(self,
                             expected_unmoved_files=frozenset(),
                             expected_unmoved_dirs=frozenset()):
    """Ensures that |RenameBreakpadFiles| doesn't move files/dirs.

    Automatically adds |self.breakpad_unzip_dir| to
    |expected_unmoved_dirs| since this file should never be moved.
    """
    breakpad_unzip_dir = {self.breakpad_unzip_dir}
    expected_unmoved_dirs = expected_unmoved_dirs.union(breakpad_unzip_dir)

    unmoved_dirs, unmoved_files = self._listSubtree(self.breakpad_dir)
    self.assertEqual(expected_unmoved_files, unmoved_files)
    self.assertEqual(expected_unmoved_dirs, unmoved_dirs)

  def _assertFilesInOutputDir(self, expected_moved_files=frozenset()):
    """Ensures that |RenameBreakpadFiles| correctly moves files.
    """
    moved_files = set(os.listdir(self.breakpad_output_dir))
    self.assertEqual(expected_moved_files, moved_files)

  def testRenameBreakpadFiles(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'34984AB4EF948C.breakpad', '29E6F9A7CE00F.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesUpperCaseName(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 12a345b6c7def890 name1.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'12A345B6C7DEF890.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesLinuxx86_64(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad.x64')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    self._assertFilesInInputDir()

    expected_moved_files = {'34984AB4EF948C.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesMultipleSubdirs(self):
    subdir1 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    subdir2 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(subdir1, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir2, 'file2.breakpad')
    breakpad_file3 = os.path.join(self.breakpad_unzip_dir, 'file3.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 48537ABD name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 38ABC9F name2.so')
    with open(breakpad_file3, 'w') as file3:
      file3.write('MODULE mac x86_64 45DFE name3.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_dirs = {subdir1, subdir2}
    self._assertFilesInInputDir(expected_unmoved_dirs=expected_unmoved_dirs)

    expected_moved_files = {
        '48537ABD.breakpad', '38ABC9F.breakpad', '45DFE.breakpad'
    }
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesNonBreakpad(self):
    valid_file = os.path.join(self.breakpad_unzip_dir, 'valid.breakpad')
    fake_file = os.path.join(self.breakpad_unzip_dir, 'fake.gz')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.json')
    random_dir = tempfile.mkdtemp(dir=self.breakpad_dir)
    non_breakpad_file = os.path.join(random_dir, 'random.txt')

    with open(valid_file, 'w') as file1:
      file1.write('MODULE mac x86_64 329FDEA987BC name.so')
    with open(fake_file, 'w') as file2:
      file2.write('random text blah blah blah')
    with open(empty_file, 'w'):
      pass
    with open(non_breakpad_file, 'w') as file3:
      file3.write('MODULE mac x86_64 329FDEA987BC name.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {fake_file, empty_file, non_breakpad_file}
    expected_unmoved_dirs = {random_dir}
    self._assertFilesInInputDir(expected_unmoved_files, expected_unmoved_dirs)

    expected_moved_files = {'329FDEA987BC.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesInvalidBreakpad(self):
    valid_file = os.path.join(self.breakpad_unzip_dir, 'valid.breakpad')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.breakpad')
    no_module_file = os.path.join(self.breakpad_unzip_dir, 'no-module.breakpad')
    short_file = os.path.join(self.breakpad_unzip_dir, 'short.breakpad')

    with open(valid_file, 'w') as file1:
      file1.write('MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name')
    with open(empty_file, 'w'):
      pass
    with open(no_module_file, 'w') as file2:
      file2.write('NOTMODULE mac x86_64 1240DF90E9AC39038EF400 name')
    with open(short_file, 'w') as file3:
      file3.write('MODULE mac 1240DF90E9AC39038EF400 name')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {empty_file, no_module_file, short_file}
    self._assertFilesInInputDir(expected_unmoved_files)

    expected_moved_files = {'1240DF90E9AC39038EF400.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesOnlyNonBreakpadAndMisformat(self):
    fake_file = os.path.join(self.breakpad_unzip_dir, 'fake.breakpad')
    empty_file = os.path.join(self.breakpad_unzip_dir, 'empty.breakpad')
    no_module_file = os.path.join(self.breakpad_unzip_dir, 'no-module.breakpad')
    short_file = os.path.join(self.breakpad_unzip_dir, 'short.breakpad')
    random_dir = tempfile.mkdtemp(dir=self.breakpad_dir)
    non_breakpad_file = os.path.join(random_dir, 'random.txt')

    with open(fake_file, 'w') as file1:
      file1.write('random text blah blah blah')
    with open(empty_file, 'w'):
      pass
    with open(no_module_file, 'w') as file2:
      file2.write('NOTMODULE mac x86_64 1240DF90E9AC39038EF400 name')
    with open(short_file, 'w') as file3:
      file3.write('MODULE mac 1240DF90E9AC39038EF400 name')
    with open(non_breakpad_file, 'w') as file4:
      file4.write('MODULE mac x86_64 1240DF90E9AC39038EF400 Chrome Name')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_output_dir)

    expected_unmoved_files = {
        fake_file, non_breakpad_file, empty_file, no_module_file, short_file
    }
    expected_unmoved_dirs = {random_dir}
    self._assertFilesInInputDir(expected_unmoved_files, expected_unmoved_dirs)

    self._assertFilesInOutputDir()

  def testRenameBreakpadFilesRepeatedModuleID(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE mac x86_64 12ABC8987DE name.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 12ABC8987DE name.so')

    # Check that the right exception is raised.
    exception_msg = ('Symbol file modules ids are not unique')

    with self.assertRaises(AssertionError) as e:
      rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                          self.breakpad_output_dir)
    self.assertIn(exception_msg, str(e.exception))

    # Check breakpad file with repeated module id is not moved. More
    # complicated because either of the breakpad files could be moved.
    self.assertTrue(
        os.path.isfile(breakpad_file1) ^ os.path.isfile(breakpad_file2))

    # Ensure one of the breakpad module files got moved. No matter which
    # breakpad file got moved, it will have the same new module-based filename.
    expected_moved_files = {'12ABC8987DE.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesRepeatedModuleIDMultipleSubdirs(self):
    subdir1 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    subdir2 = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(subdir1, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir2, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE mac x86_64 ABCE4853004895 name.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 ABCE4853004895 name.so')

    # Check that the right exception is raised.
    exception_msg = ('Symbol file modules ids are not unique')
    with self.assertRaises(AssertionError) as e:
      rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                          self.breakpad_output_dir)
    self.assertIn(exception_msg, str(e.exception))

    # Check breakpad file with repeated module id is not moved. More
    # complicated because either of the breakpad files could be moved.
    self.assertTrue(
        os.path.isfile(breakpad_file1) ^ os.path.isfile(breakpad_file2))

    # Ensure one of the breakpad module files got moved. No matter which
    # breakpad file got moved, it will have the same new module-based filename.
    expected_moved_files = {'ABCE4853004895.breakpad'}
    self._assertFilesInOutputDir(expected_moved_files)

  def testRenameBreakpadFilesInputDirEqualsOutputDir(self):
    subdir = tempfile.mkdtemp(dir=self.breakpad_unzip_dir)
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(subdir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir, self.breakpad_dir)

    # All files should be moved into |self.breakpad_dir|.
    moved_breakpad1 = os.path.join(self.breakpad_dir, '34984AB4EF948C.breakpad')
    moved_breakpad2 = os.path.join(self.breakpad_dir, '29E6F9A7CE00F.breakpad')
    expected_files = {moved_breakpad1, moved_breakpad2}
    expected_unmoved_dirs = {subdir}
    self._assertFilesInInputDir(expected_files, expected_unmoved_dirs)

    # No files should be moved to |self.breakpad_output_dir|.
    self._assertFilesInOutputDir()

  def testRenameBreakpadFilesAllUnmoved(self):
    breakpad_file1 = os.path.join(self.breakpad_unzip_dir, 'file1.breakpad')
    breakpad_file2 = os.path.join(self.breakpad_unzip_dir, 'file2.breakpad')

    with open(breakpad_file1, 'w') as file1:
      file1.write('MODULE Linux x86_64 34984AB4EF948C name1.so')
    with open(breakpad_file2, 'w') as file2:
      file2.write('MODULE mac x86_64 29e6f9a7ce00f name2.so')

    rename_breakpad.RenameBreakpadFiles(self.breakpad_dir,
                                        self.breakpad_unzip_dir)

    # All files should be renamed but not moved.
    unmoved_breakpad1 = os.path.join(self.breakpad_unzip_dir,
                                     '34984AB4EF948C.breakpad')
    unmoved_breakpad2 = os.path.join(self.breakpad_unzip_dir,
                                     '29E6F9A7CE00F.breakpad')
    expected_renamed_files = {unmoved_breakpad1, unmoved_breakpad2}
    self._assertFilesInInputDir(expected_renamed_files)

    # No files should be moved to |self.breakpad_output_dir|.
    self._assertFilesInOutputDir()


if __name__ == '__main__':
  unittest.main()
