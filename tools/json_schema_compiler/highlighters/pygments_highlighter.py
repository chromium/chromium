# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
try:
  import pygments
  from pygments.lexers import CppLexer
  from pygments.formatters import HtmlFormatter
  PYGMENTS_IMPORTED = True
except ImportError:
  print('It appears that Pygments is not installed. '
    'Can be installed using easy_install Pygments or from http://pygments.org.')
  PYGMENTS_IMPORTED = False

class PygmentsHighlighter(object):
  def __init__(self):
    if not PYGMENTS_IMPORTED:
      raise ImportError('Pygments not installed')

  """Highlighter that uses the python pygments library to highlight code.
  """
  def GetCSS(self, style):
    formatter = HtmlFormatter(linenos=True,
        style=pygments.styles.get_style_by_name(style))
    return formatter.get_style_defs('.highlight')

  def GetCodeElement(self, code, style):
    formatter = HtmlFormatter(linenos=True,
            style=pygments.styles.get_style_by_name(style))
    return pygments.highlight(code, CppLexer(), formatter)

  def DisplayName(self):
    return 'pygments' + ('' if PYGMENTS_IMPORTED else ' (not installed)')

  def GetStyles(self):
    return list(pygments.styles.get_all_styles())
