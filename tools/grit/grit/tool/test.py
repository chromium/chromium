# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from grit.tool import interface

class TestTool(interface.Tool):
  '''This tool does nothing except print out the global options and
tool-specific arguments that it receives.  It is intended only for testing,
hence the name :)
'''

  def ShortDescription(self):
    return 'A do-nothing tool for testing command-line parsing.'

  def Run(self, global_options, my_arguments):
    print('NOTE This tool is only for testing the parsing of global options and')
    print('tool-specific arguments that it receives.  You may have intended to')
    print('run "grit unit" which is the unit-test suite for GRIT.')
    print('Options: %s' % repr(global_options))
    print('Arguments: %s' % repr(my_arguments))
    return 0
