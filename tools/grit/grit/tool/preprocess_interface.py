# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Base class for preprocessing of RC files.
'''

from __future__ import print_function

class PreProcessor(object):
  ''' Base class for preprocessing of the RC file data before being
  output through the RC2GRD tool. You should implement this class if
  you have specific constructs in your RC files that GRIT cannot handle.'''


  def Process(self, rctext, rcpath):
    ''' Processes the data in rctext.
    Args:
      rctext: string containing the contents of the RC file being processed
      rcpath: the path used to access the file.

    Return:
      The processed text.
    '''
    raise NotImplementedError()
