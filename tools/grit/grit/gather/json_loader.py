# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from grit.gather import interface


class JsonLoader(interface.GathererBase):
  '''A simple gatherer that loads and parses a JSON file.'''

  def Parse(self):
    '''Reads and parses the text of self._json_text into the data structure in
    self._data.
    '''
    self._json_text = self._LoadInputFile()
    self._data = None

    globs = {}
    exec('data = ' + self._json_text, globs)
    self._data = globs['data']

  def GetData(self):
    '''Returns the parsed JSON data.'''
    return self._data
