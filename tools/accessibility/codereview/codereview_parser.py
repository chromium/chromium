# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parser for a comments file."""

import re

COMMENTS_STR = "=== COMMENTS ===\n"
COMMENT_SEP_STR = "=" * 72 + "\n"
DASHES_STR = "-" * 36 + "\n"
FILE_LINE_RE = re.compile(
    "File (?P<name>[^\s]+)( \(snapshot (?P<snapshot>\d+)\))?")
LINE_LINE_RE = re.compile("Line (?P<number>\d+): (?P<text>.*)\n")


class Parser(object):

  def __init__(self, linesiter):
    self._lines = linesiter

  def OnPreambleLine(self, line):
    pass

  def OnFinishPreamble(self):
    pass

  def OnOverallComment(self, comment):
    pass

  def OnFileComment(self, path, line, text, comment):
    pass

  def OnError(self, msg):
    pass

  def Parse(self):
    self.ReadPreamble()
    self.OnFinishPreamble()
    self.ReadOverallComment()
    self.ReadFileComments()

  def ReadPreamble(self):
    for line in self._lines:
      if line == COMMENTS_STR:
        break
      self.OnPreambleLine(line)
    else:
      self.OnError("No COMMENTS line found")

  def ReadOverallComment(self):
    self.OnOverallComment(self.ReadComment())

  def ReadComment(self):
    ret = ""
    for line in self._lines:
      if line == COMMENT_SEP_STR:
        break
      ret += line
    else:
      self.OnError("No final separator line")
    return ret

  def ReadFileComments(self):
    while self.ReadFileComment():
      pass

  def ReadFileComment(self):
    try:
      path = self.ReadFileLine()
    except StopIteration:
      return False
    self.SkipDashes()
    line, text = self.ReadLineLine()
    comment = self.ReadComment()
    self.OnFileComment(path, line, text, comment)
    return True

  def ReadFileLine(self):
    line = next(self._lines)
    match = FILE_LINE_RE.search(line)
    if not match:
      self.OnError("Invalid file line: %s" % line)
    return match.group("name")

  def SkipDashes(self):
    line = next(self._lines)
    if line != DASHES_STR:
      self.OnError("Expecting dashes: %s" % line)

  def ReadLineLine(self):
    line = next(self._lines)
    match = LINE_LINE_RE.match(line)
    if not match:
      self.OnError("Invalid line line: %s " % line)
    return int(match.group("number")), match.group("text")
