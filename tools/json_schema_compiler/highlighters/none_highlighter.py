# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi

class NoneHighlighter(object):
  """Highlighter that just wraps code in a <pre>.
  """
  def GetCSS(self, style):
    return ''

  def GetCodeElement(self, code, style):
    return '<pre>' + cgi.escape(code) + '</pre>'

  def DisplayName(self):
    return 'none'

  def GetStyles(self):
    return []
