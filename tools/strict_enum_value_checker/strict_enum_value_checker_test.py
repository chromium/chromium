#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import os
import re
import unittest

from strict_enum_value_checker import StrictEnumValueChecker

class MockLogging(object):
  def __init__(self):
    self.lines = []

  def info(self, message):
    self.lines.append(message)

  def debug(self, message):
    self.lines.append(message)

class MockInputApi(object):
  def __init__(self):
    self.re = re
    self.os_path = os.path
    self.files = []
    self.is_committing = False
    self.logging = MockLogging()

  def AffectedFiles(self, include_deletes=None):
    return self.files


class MockOutputApi(object):
  class PresubmitResult(object):
    def __init__(self, message, items=None, long_text=""):
      self.message = message
      self.items = items
      self.long_text = long_text

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items, long_text=""):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = "error"

  class PresubmitPromptWarning(PresubmitResult):
    def __init__(self, message, items, long_text=""):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = "warning"

  class PresubmitNotifyResult(PresubmitResult):
    def __init__(self, message, items, long_text=""):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = "notify"


class MockFile(object):
  def __init__(self, local_path, old_contents, new_contents):
    self._local_path = local_path
    self._new_contents = new_contents
    self._old_contents = old_contents
    self._cached_changed_contents = None

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path

  def IsDirectory(self):
    return False

  def GenerateScmDiff(self):
    result = ""
    for line in difflib.unified_diff(self._old_contents, self._new_contents,
                                     self._local_path, self._local_path):
      result += line
    return result

  # NOTE: This method is a copy of ChangeContents method of AffectedFile in
  # presubmit_support.py
  def ChangedContents(self):
    """Returns a list of tuples (line number, line text) of all new lines.

     This relies on the scm diff output describing each changed code section
     with a line of the form

     ^@@ <old line num>,<old size> <new line num>,<new size> @@$
    """
    if self._cached_changed_contents is not None:
      return self._cached_changed_contents[:]
    self._cached_changed_contents = []
    line_num = 0

    if self.IsDirectory():
      return []

    for line in self.GenerateScmDiff().splitlines():
      m = re.match(r"^@@ [0-9\,\+\-]+ \+([0-9]+)\,[0-9]+ @@", line)
      if m:
        line_num = int(m.groups(1)[0])
        continue
      if line.startswith("+") and not line.startswith("++"):
        self._cached_changed_contents.append((line_num, line[1:]))
      if not line.startswith("-"):
        line_num += 1
    return self._cached_changed_contents[:]


class MockChange(object):
  def __init__(self, changed_files):
    self._changed_files = changed_files

  def LocalPaths(self):
    return self._changed_files


class StrictEnumValueCheckerTest(unittest.TestCase):
  TEST_FILE_PATTERN = "changed_file_%s.h"
  MOCK_FILE_LOCAL_PATH = "mock_enum.h"
  START_MARKER = "enum MockEnum {"
  END_MARKER = "  mBoundary"

  def _ReadTextFileContents(self, path):
    """Given a path, returns a list of strings corresponding to the text lines
    in the file. Reads files in text format.

    """
    fo = open(path, "r")
    try:
      contents = fo.readlines()
    finally:
      fo.close()
    return contents

  def _ReadInputFile(self):
    return self._ReadTextFileContents("mock_enum.h")

  def _PrepareTest(self, new_file_path):
    old_contents = self._ReadInputFile()
    if not new_file_path:
      new_contents = []
    else:
      new_contents = self._ReadTextFileContents(new_file_path)
    input_api = MockInputApi()
    mock_file = MockFile(self.MOCK_FILE_LOCAL_PATH,
                         old_contents,
                         new_contents)
    input_api.files.append(mock_file)
    output_api = MockOutputApi()
    return input_api, output_api

  def _RunTest(self, new_file_path):
    input_api, output_api = self._PrepareTest(new_file_path)
    checker = StrictEnumValueChecker(input_api, output_api, self.START_MARKER,
                                     self.END_MARKER, self.MOCK_FILE_LOCAL_PATH)
    results = checker.Run()
    return results

  def testDeleteFile(self):
    results = self._RunTest(new_file_path=None)
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a single warning about file deletion.")

  def testSimpleValidEdit(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "1")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(0, len(results),
                      "We should get no warning for simple edits.")

  def testSingleDeletionOfEntry(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "2")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for an entry deletion.")

  def testSingleRenameOfEntry(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "3")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for an entry rename, even "
                      "though it is not optimal.")

  def testMissingEnumStartOfEntry(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "4")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for a missing enum marker.")

  def testMissingEnumEndOfEntry(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "5")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for a missing enum marker.")

  def testInvertedEnumMarkersOfEntry(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "6")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for inverted enum markers.")

  def testMultipleInvalidEdits(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "7")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(3, len(results),
                      "We should get 3 warnings (one per edit).")

  def testSingleInvalidInserts(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "8")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(1, len(results),
                      "We should get a warning for a single invalid "
                      "insertion inside the enum.")

  def testMulitpleValidInserts(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "9")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(0, len(results),
                      "We should not get a warning mulitple valid edits")

  def testSingleValidDeleteOutsideOfEnum(self):
    results = self._RunTest(self.TEST_FILE_PATTERN % "10")
    # TODO(rpaquay) How to check it's the expected warning?'
    self.assertEquals(0, len(results),
                      "We should not get a warning for a deletion outside of "
                      "the enum")


if __name__ == '__main__':
  unittest.main()
