# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A CustomType for filenames.'''

from __future__ import print_function

from grit import clique
from grit import lazy_re


class WindowsFilename(clique.CustomType):
  '''Validates that messages can be used as Windows filenames, and strips
  illegal characters out of translations.
  '''

  BANNED = lazy_re.compile(r'\+|:|\/|\\\\|\*|\?|\"|\<|\>|\|')

  def Validate(self, message):
    return not self.BANNED.search(message.GetPresentableContent())

  def ValidateAndModify(self, lang, translation):
    is_ok = self.Validate(translation)
    self.ModifyEachTextPart(lang, translation)
    return is_ok

  def ModifyTextPart(self, lang, text):
    return self.BANNED.sub(' ', text)
