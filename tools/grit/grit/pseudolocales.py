# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pseudolocale translations for chrome."""


import re
import string

from collections import namedtuple
from grit import lazy_re
from grit import tclib

ACCENTED_STRINGS = {
    '!': '\u00a1',
    '$': '\u20ac',
    '?': '\u00bf',
    'A': '\u00c5',
    'C': '\u00c7',
    'D': '\u00d0',
    'E': '\u00c9',
    'G': '\u011c',
    'H': '\u0124',
    'I': '\u00ce',
    'J': '\u0134',
    'K': '\u0136',
    'L': '\u013b',
    'N': '\u00d1',
    'O': '\u00d6',
    'P': '\u00de',
    'R': '\u00ae',
    'S': '\u0160',
    'T': '\u0162',
    'U': '\u00db',
    'W': '\u0174',
    'Y': '\u00dd',
    'Z': '\u017d',
    'a': '\u00e5',
    'c': '\u00e7',
    'd': '\u00f0',
    'e': '\u00e9',
    'f': '\u0192',
    'g': '\u011d',
    'h': '\u0125',
    'i': '\u00ee',
    'j': '\u0135',
    'k': '\u0137',
    'l': '\u013c',
    'n': '\u00f1',
    'o': '\u00f6',
    'p': '\u00fe',
    's': '\u0161',
    't': '\u0163',
    'u': '\u00fb',
    'w': '\u0175',
    'y': '\u00fd',
    'z': '\u017e',
}

NUMBERS = [
    '- one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine',
    'ten'
]
PLACEHOLDER_STRING = '{PLACEHOLDER_VARIABLE}'
ALPHABETIC_RUN = lazy_re.compile(r'([^\W0-9_]+)')
WORD = lazy_re.compile(r'\b\S+\b')

# RTL modifiers for letters
RLO = '\u202e'
PDF = '\u202c'


class Node:
  """A node in the syntax tree representing a message to be translated."""

  translatable = False
  after = ''

  def __init__(self, text, children=None):
    self.text = text
    self.children = [] if children is None else children

  def GetNumWords(self):
    """Returns an approximate worst-case (maximum) number of words within the
    tree."""
    return sum(child.GetNumWords() for child in self.children)

  def Transform(self, fn):
    """Modifies the tree by applying fn to any translatable text within the tree

    Args:
      fn: Callable[[unicode], unicode]
    """
    for child in self.children:
      child.Transform(fn)

  def ToString(self):
    """Returns a string representation of the tree suitable for creating a
    translation from.
    """
    children = ''.join(c.ToString() for c in self.children)
    return '%s%s%s' % (self.text, children, self.after)

  def __repr__(self):
    # For debugging
    if self.children:
      child_lines = '\n'.join('  ' + line for node in self.children
                              for line in repr(node).split('\n'))
      return '%s[before=%s, after=%s\n%s\n]' % (self.__class__.__name__,
                                                repr(self.text), repr(
                                                    self.after), child_lines)
    else:
      return '%s %s' % (self.__class__.__name__, repr(self.text))

  @classmethod
  def _MatchPattern(cls, text):
    match = cls.pattern.match(text)
    if match is not None:
      return cls(match.group(0)), text[len(match.group(0)):]
    return None, text

  @classmethod
  def Parse(cls, text):
    """Matches the node against the text, consuming any part of the text that
    matches.

    Args:
      text: str

    Return: (Optional[Node], str)
      If the text starts with something matching the node, returns
        (node, leftover).
      Otherwise, returns (None, text)
    """
    return cls._MatchPattern(text)


class HtmlTag(Node):
  """HTMLTag represents a HTML tag (eg. <a href='...'> or </span>).
  Note that since we don't care about the HTML structure, this does not
  form a tree, has no children, and no linking between open and close tags.

  Lex text so that we don't interfere with html tokens.
  This lexing scheme will handle all well formed tags, html or xhtml.
  It will not handle comments, CDATA sections, or the unescaping tags:
  script, style, xmp or listing.  If any of those appear in messages,
  something is wrong.
  """
  pattern = lazy_re.compile(
      r'^</?[a-z]\w*'  # beginning of tag
      r'(?:\s+\w+(?:\s*=\s*'  # attribute start
      r'(?:[^\s"\'>]+|"[^\"]*"|\'[^\']*\'))?'  # attribute value
      r')*\s*/?>',
      re.S | re.I)


class RawText(Node):
  """RawText represents regular text able to be translated."""
  # Raw text can have a < or $ in it, but only at the very start.
  # This guarantees that it's already tried and failed to match an HTML tag
  # and variable.
  pattern = lazy_re.compile(r'^[^{}][^{}<$]*', re.S)

  def GetNumWords(self):
    return len(WORD.findall(self.text))

  def Transform(self, fn):
    self.text = fn(self.text)


class BasicVariable(Node):
  """Represents a variable. Usually used inside a plural option, but has been
  overloaded to store placeholders as well.
  """
  pattern = lazy_re.compile(r'^\$?{[a-zA-Z0-9_]+}')

  def GetNumWords(self):
    return 1


class PluralOption(Node):
  """Represents a single option for a plural selection.
  eg. =1 {singular option here}
  """
  pattern = lazy_re.compile(r'^(=[0-9]+|other)\s*{')
  after = '}\n'

  @classmethod
  def Parse(cls, text):
    node, text = cls._MatchPattern(text)
    assert node is not None, text
    child, text = NodeSequence.Parse(text)
    assert child is not None, text
    node.children = child.children if isinstance(child,
                                                 NodeSequence) else [child]

    assert text.startswith('}')
    return node, text[1:]


class Plural(Node):
  """Represents a set of options for plurals.
  eg. {VARIABLE, plural, =1 {singular} other {plural}}
  """
  pattern = lazy_re.compile(r'^{[A-Za-z0-9_]+,\s*plural,\s*(offset:\d+\s*)?',
                            re.S)
  after = '}'

  @classmethod
  def Parse(cls, text):
    node, text = cls._MatchPattern(text)
    if node is None:
      return None, text
    while not text.startswith('}'):
      child, text = PluralOption.Parse(text)
      assert child is not None, text
      node.children.append(child)
      text = text.lstrip()

    assert text.startswith('}'), text
    return node, text[1:]

  def GetNumWords(self):
    return max(child.GetNumWords() for child in self.children)


class NodeSequence(Node):
  """Represents a series of nodes.
  eg. hello {VAR} -> NodeSequence([RawText('Hello'), BasicVariable('{VAR}'])"""
  child_types = [HtmlTag, BasicVariable, Plural, RawText]

  def __init__(self, children):
    super().__init__('', children)

  @classmethod
  def Parse(cls, text):
    children = []
    orig_text = None
    while text != orig_text:
      orig_text = text
      for node in cls.child_types:
        child, text = node.Parse(text)
        if child is not None:
          children.append(child)
          break
    assert children, text
    if len(children) == 1:
      return children[0], text
    return cls(children), text


def BuildTree(text):
  """Builds a tree from some text"""
  root, leftovers = NodeSequence.Parse(text)
  assert not leftovers, leftovers
  return root


def BuildTreeFromMessage(message):
  """Builds a tree from message, substituting any placeholders with
  PLACEHOLDER_STRING. Returns (tree, substituted placeholders)
  """
  text = ''
  placeholders = []
  for part in message.GetContent():
    if isinstance(part, tclib.Placeholder):
      text += PLACEHOLDER_STRING
      placeholders.append(part)
    else:
      text += part
  return BuildTree(text), placeholders


def ToTranslation(tree, placeholders):
  """Converts the tree back to a translation, substituting the placeholders
  back in as required.
  """
  text = tree.ToString()
  assert text.count(PLACEHOLDER_STRING) == len(placeholders)
  transl = tclib.Translation()
  for placeholder in placeholders:
    index = text.find(PLACEHOLDER_STRING)
    if index > 0:
      transl.AppendText(text[:index])
    text = text[index + len(PLACEHOLDER_STRING):]
    transl.AppendPlaceholder(placeholder)
  if text:
    transl.AppendText(text)
  return transl


def PseudoLongStringMessage(message):
  """Returns a pseudo-long string (en-XA) translation of the provided message.

  Args:
    message: tclib.Message()

  Return:
    tclib.Translation()
  """

  tree, placeholders = BuildTreeFromMessage(message)
  # This will change after the transformation, so do it early.
  n_words = tree.GetNumWords()
  tree.Transform(lambda x: ''.join(
      ACCENTED_STRINGS.get(letter, letter) for letter in x))
  transl = ToTranslation(tree, placeholders)
  transl.AppendText(' ' + ' '.join(NUMBERS[i % len(NUMBERS)]
                                   for i in range(n_words)))

  return transl


def PseudoRTLMessage(message):
  """Returns a pseudo-RTL (ar-XB) translation of the provided message.

  Args:
    message: tclib.Message()

  Return:
    tclib.Translation()
  """
  tree, placeholders = BuildTreeFromMessage(message)
  tree.Transform(lambda text: ALPHABETIC_RUN.sub(
      lambda run: RLO + run.group() + PDF, text))
  return ToTranslation(tree, placeholders)
