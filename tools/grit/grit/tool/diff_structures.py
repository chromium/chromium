# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit sdiff' tool.
'''

from __future__ import print_function

import os
import getopt
import sys
import tempfile

from grit.node import structure
from grit.tool import interface

from grit import constants
from grit import util

# Builds the description for the tool (used as the __doc__
# for the DiffStructures class).
_class_doc = """\
Allows you to view the differences in the structure of two files,
disregarding their translateable content.  Translateable portions of
each file are changed to the string "TTTTTT" before invoking the diff program
specified by the P4DIFF environment variable.

Usage: grit sdiff [-t TYPE] [-s SECTION] [-e ENCODING] LEFT RIGHT

LEFT and RIGHT are the files you want to diff.  SECTION is required
for structure types like 'dialog' to identify the part of the file to look at.
ENCODING indicates the encoding of the left and right files (default 'cp1252').
TYPE can be one of the following, defaults to 'tr_html':
"""
for gatherer in structure._GATHERERS:
  _class_doc += " - %s\n" % gatherer


class DiffStructures(interface.Tool):
  __doc__ = _class_doc

  def __init__(self):
    self.section = None
    self.left_encoding = 'cp1252'
    self.right_encoding = 'cp1252'
    self.structure_type = 'tr_html'

  def ShortDescription(self):
    return 'View differences without regard for translateable portions.'

  def Run(self, global_opts, args):
    (opts, args) = getopt.getopt(args, 's:e:t:',
                                 ('help', 'left_encoding=', 'right_encoding='))
    for key, val in opts:
      if key == '-s':
        self.section = val
      elif key == '-e':
        self.left_encoding = val
        self.right_encoding = val
      elif key == '-t':
        self.structure_type = val
      elif key == '--left_encoding':
        self.left_encoding = val
      elif key == '--right_encoding':
        self.right_encoding == val
      elif key == '--help':
        self.ShowUsage()
        sys.exit(0)

    if len(args) != 2:
      print("Incorrect usage - 'grit help sdiff' for usage details.")
      return 2

    if 'P4DIFF' not in os.environ:
      print("Environment variable P4DIFF not set; defaulting to 'windiff'.")
      diff_program = 'windiff'
    else:
      diff_program = os.environ['P4DIFF']

    left_trans = self.MakeStaticTranslation(args[0], self.left_encoding)
    try:
      try:
        right_trans = self.MakeStaticTranslation(args[1], self.right_encoding)

        os.system('%s %s %s' % (diff_program, left_trans, right_trans))
      finally:
        os.unlink(right_trans)
    finally:
      os.unlink(left_trans)

  def MakeStaticTranslation(self, original_filename, encoding):
    """Given the name of the structure type (self.structure_type), the filename
    of the file holding the original structure, and optionally the "section" key
    identifying the part of the file to look at (self.section), creates a
    temporary file holding a "static" translation of the original structure
    (i.e. one where all translateable parts have been replaced with "TTTTTT")
    and returns the temporary file name.  It is the caller's responsibility to
    delete the file when finished.

    Args:
      original_filename: 'c:\\bingo\\bla.rc'

    Return:
      'c:\\temp\\werlkjsdf334.tmp'
    """
    original = structure._GATHERERS[self.structure_type](original_filename,
                                                         extkey=self.section,
                                                         encoding=encoding)
    original.Parse()
    translated = original.Translate(constants.CONSTANT_LANGUAGE, False)

    fname = tempfile.mktemp()
    with util.WrapOutputStream(open(fname, 'wb')) as writer:
      writer.write("Original filename: %s\n=============\n\n"
                       % original_filename)
      writer.write(translated)  # write in UTF-8

    return fname
