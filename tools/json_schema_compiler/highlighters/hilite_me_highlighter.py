# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urllib
import urllib2

class HiliteMeHighlighter(object):
  """Highlighter that calls the http://hilite.me API to highlight code.
  """
  def GetCSS(self, style):
    return ''

  def GetCodeElement(self, code, style):
    # Call hilite.me API to do syntax highlighting
    return urllib2.urlopen('http://hilite.me/api',
        urllib.urlencode([
            ('code', code),
            ('lexer', 'cpp'),
            ('style', style),
            ('linenos', 1)])
    ).read()

  def DisplayName(self):
    return 'hilite.me (slow, requires internet)'

  def GetStyles(self):
    return ['monokai', 'manni', 'perldoc', 'borland', 'colorful', 'default',
        'murphy', 'vs', 'trac', 'tango', 'fruity', 'autumn', 'bw', 'emacs',
        'vim', 'pastie', 'friendly', 'native']
