#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test that checks preprocessing of files.
   Tests preprocessing by adding having the preprocessor
   provide the actual rctext data.
'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

import grit.tool.preprocess_interface
from grit.tool import rc2grd


class PreProcessingUnittest(unittest.TestCase):

  def testPreProcessing(self):
    tool = rc2grd.Rc2Grd()
    class DummyOpts:
      verbose = False
      extra_verbose = False
    tool.o = DummyOpts()
    tool.pre_process = 'grit.tool.preprocess_unittest.DummyPreProcessor'
    result = tool.Process('', '.\resource.rc')

    self.assertTrue(
      result.children[2].children[2].children[0].attrs['name'] == 'DUMMY_STRING_1')

class DummyPreProcessor(grit.tool.preprocess_interface.PreProcessor):
  def Process(self, rctext, rcpath):
    rctext = '''STRINGTABLE
BEGIN
  DUMMY_STRING_1         "String 1"
  // Some random description
  DUMMY_STRING_2        "This text was added during preprocessing"
END
    '''
    return rctext

if __name__ == '__main__':
  unittest.main()
