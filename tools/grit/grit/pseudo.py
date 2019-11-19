# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Pseudotranslation support.  Our pseudotranslations are based on the
P-language, which is a simple vowel-extending language.  Examples of P:
  - "hello" becomes "hepellopo"
  - "howdie" becomes "hopowdiepie"
  - "because" becomes "bepecaupause" (but in our implementation we don't
    handle the silent e at the end so it actually would return "bepecaupausepe"

The P-language has the excellent quality of increasing the length of text
by around 30-50% which is great for pseudotranslations, to stress test any
GUI layouts etc.

To make the pseudotranslations more obviously "not a translation" and to make
them exercise any code that deals with encodings, we also transform all English
vowels into equivalent vowels with diacriticals on them (rings, acutes,
diaresis, and circumflex), and we write the "p" in the P-language as a Hebrew
character Qof.  It looks sort of like a latin character "p" but it is outside
the latin-1 character set which will stress character encoding bugs.
'''

from __future__ import print_function

from grit import lazy_re
from grit import tclib


# An RFC language code for the P pseudolanguage.
PSEUDO_LANG = 'x-P-pseudo'

# Hebrew character Qof.  It looks kind of like a 'p' but is outside
# the latin-1 character set which is good for our purposes.
# TODO(joi) For now using P instead of Qof, because of some bugs it used.  Find
# a better solution, i.e. one that introduces a non-latin1 character into the
# pseudotranslation.
#_QOF = u'\u05e7'
_QOF = u'P'

# How we map each vowel.
_VOWELS = {
  u'a' : u'\u00e5',  # a with ring
  u'e' : u'\u00e9',  # e acute
  u'i' : u'\u00ef',  # i diaresis
  u'o' : u'\u00f4',  # o circumflex
  u'u' : u'\u00fc',  # u diaresis
  u'y' : u'\u00fd',  # y acute
  u'A' : u'\u00c5',  # A with ring
  u'E' : u'\u00c9',  # E acute
  u'I' : u'\u00cf',  # I diaresis
  u'O' : u'\u00d4',  # O circumflex
  u'U' : u'\u00dc',  # U diaresis
  u'Y' : u'\u00dd',  # Y acute
}
_VOWELS_KEYS = set(_VOWELS.keys())

# Matches vowels and P
_PSUB_RE = lazy_re.compile("(%s)" % '|'.join(_VOWELS_KEYS | {'P'}))


# Pseudotranslations previously created.  This is important for performance
# reasons, especially since we routinely pseudotranslate the whole project
# several or many different times for each build.
_existing_translations = {}


def MapVowels(str, also_p = False):
  '''Returns a copy of 'str' where characters that exist as keys in _VOWELS
  have been replaced with the corresponding value.  If also_p is true, this
  function will also change capital P characters into a Hebrew character Qof.
  '''
  def Repl(match):
    if match.group() == 'p':
      if also_p:
        return _QOF
      else:
        return 'p'
    else:
      return _VOWELS[match.group()]
  return _PSUB_RE.sub(Repl, str)


def PseudoString(str):
  '''Returns a pseudotranslation of the provided string, in our enhanced
  P-language.'''
  if str in _existing_translations:
    return _existing_translations[str]

  outstr = u''
  ix = 0
  while ix < len(str):
    if str[ix] not in _VOWELS_KEYS:
      outstr += str[ix]
      ix += 1
    else:
      # We want to treat consecutive vowels as one composite vowel.  This is not
      # always accurate e.g. in composite words but good enough.
      consecutive_vowels = u''
      while ix < len(str) and str[ix] in _VOWELS_KEYS:
        consecutive_vowels += str[ix]
        ix += 1
      changed_vowels = MapVowels(consecutive_vowels)
      outstr += changed_vowels
      outstr += _QOF
      outstr += changed_vowels

  _existing_translations[str] = outstr
  return outstr


def PseudoMessage(message):
  '''Returns a pseudotranslation of the provided message.

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
      transl.AppendText(PseudoString(part))

  return transl
