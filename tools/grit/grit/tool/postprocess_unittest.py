#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test that checks postprocessing of files.
   Tests postprocessing by having the postprocessor
   modify the grd data tree, changing the message name attributes.
'''

from __future__ import print_function

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

import grit.tool.postprocess_interface
from grit.tool import rc2grd


class PostProcessingUnittest(unittest.TestCase):

  def testPostProcessing(self):
    rctext = '''STRINGTABLE
BEGIN
  DUMMY_STRING_1         "String 1"
  // Some random description
  DUMMY_STRING_2        "This text was added during preprocessing"
END
    '''
    tool = rc2grd.Rc2Grd()
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    tool.o = DummyOpts()
    tool.post_process = 'grit.tool.postprocess_unittest.DummyPostProcessor'
    result = tool.Process(rctext, '.\resource.rc')

    self.failUnless(
      result.children[2].children[2].children[0].attrs['name'] == 'SMART_STRING_1')
    self.failUnless(
      result.children[2].children[2].children[1].attrs['name'] == 'SMART_STRING_2')

class DummyPostProcessor(grit.tool.postprocess_interface.PostProcessor):
  '''
  Post processing replaces all message name attributes containing "DUMMY" to
  "SMART".
  '''
  def Process(self, rctext, rcpath, grdnode):
    smarter = re.compile(r'(DUMMY)(.*)')
    messages = grdnode.children[2].children[2]
    for node in messages.children:
      name_attr = node.attrs['name']
      m = smarter.search(name_attr)
      if m:
         node.attrs['name'] = 'SMART' + m.group(2)
    return grdnode

if __name__ == '__main__':
  unittest.main()
