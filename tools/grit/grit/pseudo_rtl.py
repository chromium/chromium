# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Pseudo RTL, (aka Fake Bidi) support. It simply wraps each word with
Unicode RTL overrides.
More info at https://sites.google.com/a/chromium.org/dev/Home/fake-bidi
'''

from __future__ import print_function

import re

from grit import lazy_re
from grit import tclib

ACCENTED_STRINGS = {
  'a': u"\u00e5", 'e': u"\u00e9", 'i': u"\u00ee", 'o': u"\u00f6",
  'u': u"\u00fb", 'A': u"\u00c5", 'E': u"\u00c9", 'I': u"\u00ce",
  'O': u"\u00d6", 'U': u"\u00db", 'c': u"\u00e7", 'd': u"\u00f0",
  'n': u"\u00f1", 'p': u"\u00fe", 'y': u"\u00fd", 'C': u"\u00c7",
  'D': u"\u00d0", 'N': u"\u00d1", 'P': u"\u00de", 'Y': u"\u00dd",
  'f': u"\u0192", 's': u"\u0161", 'S': u"\u0160", 'z': u"\u017e",
  'Z': u"\u017d", 'g': u"\u011d", 'G': u"\u011c", 'h': u"\u0125",
  'H': u"\u0124", 'j': u"\u0135", 'J': u"\u0134", 'k': u"\u0137",
  'K': u"\u0136", 'l': u"\u013c", 'L': u"\u013b", 't': u"\u0163",
  'T': u"\u0162", 'w': u"\u0175", 'W': u"\u0174",
  '$': u"\u20ac", '?': u"\u00bf", 'R': u"\u00ae", r'!': u"\u00a1",
}

# a character set containing the keys in ACCENTED_STRINGS
# We should not accent characters in an escape sequence such as "\n".
# To be safe, we assume every character following a backslash is an escaped
# character. We also need to consider the case like "\\n", which means
# a blackslash and a character "n", we will accent the character "n".
TO_ACCENT = lazy_re.compile(
    r'[%s]|\\[a-z\\]' % ''.join(ACCENTED_STRINGS.keys()))

# Lex text so that we don't interfere with html tokens and entities.
# This lexing scheme will handle all well formed tags and entities, html or
# xhtml.  It will not handle comments, CDATA sections, or the unescaping tags:
# script, style, xmp or listing.  If any of those appear in messages,
# something is wrong.
TOKENS = [ lazy_re.compile(
                           '^%s' % pattern,  # match at the beginning of input
                           re.I | re.S  # html tokens are case-insensitive
                         )
           for pattern in
           (
            # a run of non html special characters
            r'[^<&]+',
            # a tag
            (r'</?[a-z]\w*' # beginning of tag
             r'(?:\s+\w+(?:\s*=\s*' # attribute start
             r'(?:[^\s"\'>]+|"[^\"]*"|\'[^\']*\'))?' # attribute value
             r')*\s*/?>'),
            # an entity
            r'&(?:[a-z]\w+|#\d+|#x[\da-f]+);',
            # an html special character not part of a special sequence
            r'.'
           ) ]

ALPHABETIC_RUN = lazy_re.compile(r'([^\W0-9_]+)')

RLO = u'\u202e'
PDF = u'\u202c'

def PseudoRTLString(text):
  '''Returns a fake bidirectional version of the source string. This code is
    based on accentString above, in turn copied from Frank Tang.
    '''
  parts = []
  while text:
    m = None
    for token in TOKENS:
      m = token.search(text)
      if m:
        part = m.group(0)
        text = text[len(part):]
        if part[0] not in ('<', '&'):
          # not a tag or entity, so accent
          part = ALPHABETIC_RUN.sub(lambda run: RLO + run.group() + PDF, part)
        parts.append(part)
        break
  return ''.join(parts)


def PseudoRTLMessage(message):
  '''Returns a pseudo-RTL (aka Fake-Bidi) translation of the provided message.

  Args:
    message: tclib.Message()

  Return:
    tclib.Translation()
  '''
  transl = tclib.Translation()
  for part in message.GetContent():
    if isinstance(part, tclib.Placeholder):
      transl.AppendPlaceholder(part)
    else:
      transl.AppendText(PseudoRTLString(part))

  return transl
