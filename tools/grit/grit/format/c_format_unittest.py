#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for c_format.py.
"""

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from six import StringIO

from grit import util
from grit.tool import build


class CFormatUnittest(unittest.TestCase):

  def testMessages(self):
    root = util.ParseGrdForUnittest(u"""
    <messages>
      <message name="IDS_QUESTIONS">Do you want to play questions?</message>
      <message name="IDS_QUOTES">
      "What's in a name, <ph name="NAME">%s<ex>Brandon</ex></ph>?"
      </message>
      <message name="IDS_LINE_BREAKS">
          Was that rhetoric?
No.
Statement.  Two all.  Game point.
</message>
      <message name="IDS_NON_ASCII">
         \u00f5\\xc2\\xa4\\\u00a4\\\\xc3\\xb5\u4924
      </message>
    </messages>
      """)

    buf = StringIO()
    build.RcBuilder.ProcessNode(root, DummyOutput('c_format', 'en'), buf)
    output = util.StripBlankLinesAndComments(buf.getvalue())
    self.assertEqual(u"""\
#include "resource.h"
const char* GetString(int id) {
  switch (id) {
    case IDS_QUESTIONS:
      return "Do you want to play questions?";
    case IDS_QUOTES:
      return "\\"What\\'s in a name, %s?\\"";
    case IDS_LINE_BREAKS:
      return "Was that rhetoric?\\nNo.\\nStatement.  Two all.  Game point.";
    case IDS_NON_ASCII:
      return "\\303\\265\\xc2\\xa4\\\\302\\244\\\\xc3\\xb5\\344\\244\\244";
    default:
      return 0;
  }
}""", output)


class DummyOutput(object):

  def __init__(self, type, language):
    self.type = type
    self.language = language

  def GetType(self):
    return self.type

  def GetLanguage(self):
    return self.language

  def GetOutputFilename(self):
    return 'hello.gif'

if __name__ == '__main__':
  unittest.main()
