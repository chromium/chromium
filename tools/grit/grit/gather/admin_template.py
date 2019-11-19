# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Gatherer for administrative template files.
'''

from __future__ import print_function

import re

from grit.gather import regexp
from grit import exception
from grit import lazy_re


class MalformedAdminTemplateException(exception.Base):
  '''This file doesn't look like a .adm file to me.'''
  pass


class AdmGatherer(regexp.RegexpGatherer):
  '''Gatherer for the translateable portions of an admin template.

  This gatherer currently makes the following assumptions:
  - there is only one [strings] section and it is always the last section
    of the file
  - translateable strings do not need to be escaped.
  '''

  # Finds the strings section as the group named 'strings'
  _STRINGS_SECTION = lazy_re.compile(
      r'(?P<first_part>.+^\[strings\])(?P<strings>.+)\Z',
      re.MULTILINE | re.DOTALL)

  # Finds the translateable sections from within the [strings] section.
  _TRANSLATEABLES = lazy_re.compile(
      r'^\s*[A-Za-z0-9_]+\s*=\s*"(?P<text>.+)"\s*$',
      re.MULTILINE)

  def Escape(self, text):
    return text.replace('\n', '\\n')

  def UnEscape(self, text):
    return text.replace('\\n', '\n')

  def Parse(self):
    if self.have_parsed_:
      return
    self.have_parsed_ = True

    self.text_ = self._LoadInputFile().strip()
    m = self._STRINGS_SECTION.match(self.text_)
    if not m:
      raise MalformedAdminTemplateException()
    # Add the first part, which is all nontranslateable, to the skeleton
    self._AddNontranslateableChunk(m.group('first_part'))
    # Then parse the rest using the _TRANSLATEABLES regexp.
    self._RegExpParse(self._TRANSLATEABLES, m.group('strings'))

  def GetTextualIds(self):
    return [self.extkey]
