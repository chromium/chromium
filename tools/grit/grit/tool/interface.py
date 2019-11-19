# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Base class and interface for tools.
'''

from __future__ import print_function

class Tool(object):
  '''Base class for all tools.  Tools should use their docstring (i.e. the
  class-level docstring) for the help they want to have printed when they
  are invoked.'''

  #
  # Interface (abstract methods)
  #

  def ShortDescription(self):
    '''Returns a short description of the functionality of the tool.'''
    raise NotImplementedError()

  def Run(self, global_options, my_arguments):
    '''Runs the tool.

    Args:
      global_options: object grit_runner.Options
      my_arguments: [arg1 arg2 ...]

    Return:
      0 for success, non-0 for error
    '''
    raise NotImplementedError()

  #
  # Base class implementation
  #

  def __init__(self):
    self.o = None

  def ShowUsage(self):
    '''Show usage text for this tool.'''
    print(self.__doc__)

  def SetOptions(self, opts):
    self.o = opts

  def Out(self, text):
    '''Always writes out 'text'.'''
    self.o.output_stream.write(text)

  def VerboseOut(self, text):
    '''Writes out 'text' if the verbose option is on.'''
    if self.o.verbose:
      self.o.output_stream.write(text)

  def ExtraVerboseOut(self, text):
    '''Writes out 'text' if the extra-verbose option is on.
    '''
    if self.o.extra_verbose:
      self.o.output_stream.write(text)
