# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers to parse content of xml files."""
import html
from xml.dom import minidom

from typing import Iterator

_ELEMENT_NODE = minidom.Node.ELEMENT_NODE


def GetTagSubTree(tree: minidom.Element, tag: str,
                  depth: int) -> minidom.Element:
  """Returns sub tree with tag element as a root.

  When no element with tag name is found or there are many of them
  original tree is returned.

  Args:
    tree: XML dom tree.
    tag: Element's tag name.
    depth: Defines how deep in the tree function should search for a match.

  Returns:
    xml.dom.minidom.Node: Sub tree (matching criteria) or original one.
  """
  entries = list(IterElementsWithTag(tree, tag, depth))
  if len(entries) == 1:
    tree = entries[0]
  return tree


def NormalizeString(text: str) -> str:
  r"""Replaces all white space sequences with a single space.

  Also, unescapes any HTML escaped characters, e.g. &quot; or &gt;.

  Args:
    text: The string to normalize, '\n\n a \n b&gt;c  '.

  Returns:
    The normalized string 'a b>c'.
  """
  line = ' '.join(text.split())

  # Unescape using default ASCII encoding. Unescapes any HTML escaped character
  # like &quot; etc.
  return html.unescape(line)


def NormalizeAllAttributeValues(node: minidom.Element) -> minidom.Element:
  """Recursively normalizes all tag attribute values in the given tree.

  Args:
    node: The minidom node to be normalized.

  Returns:
    The normalized minidom node.
  """
  if node.nodeType == _ELEMENT_NODE:
    for a in node.attributes.keys():
      node.attributes[a].value = NormalizeString(node.attributes[a].value)

  for c in node.childNodes:
    NormalizeAllAttributeValues(c)
  return node


def GetTextFromChildNodes(node: minidom.Element) -> str:
  """Returns a string concatenation of the text of the given node's children.

  Comments are ignored, consecutive lines of text are joined with a single
  space, and paragraphs are maintained so that long text is more readable on
  dashboards.

  Args:
    node: The DOM Element whose children's text is to be extracted, processed,
      and returned.

  Returns:
    A string concatenation of the text of the given node's children.
  """
  paragraph_break = '\n\n'
  text_parts = []

  for child in node.childNodes:
    if child.nodeType != minidom.Node.COMMENT_NODE:
      child_text = child.toxml()
      if not child_text:
        continue

      # If the given node has the below XML representation, then the text
      # added to the list is 'Some words.\n\nWords.'
      # <tag>
      #   Some
      #   words.
      #
      #   <!--Child comment node.-->
      #
      #   Words.
      # </tag>

      # In the case of the first child text node, raw_paragraphs would store
      # ['\n  Some\n  words.', '  '], and in the case of the second,
      # raw_paragraphs would store ['', '  Words.\n'].
      raw_paragraphs = child_text.split(paragraph_break)

      # In the case of the first child text node, processed_paragraphs would
      # store ['Some words.', ''], and in the case of the second,
      # processed_paragraphs would store ['Words.'].
      processed_paragraphs = [
          NormalizeString(text) for text in raw_paragraphs if text
      ]
      text_parts.append(paragraph_break.join(processed_paragraphs))

  return ''.join(text_parts).strip()


def IterElementsWithTag(root: minidom.Element,
                        tag: str,
                        depth: int = -1) -> Iterator[minidom.Element]:
  """Iterates over DOM tree and yields elements matching tag name.

  It's meant to be replacement for `getElementsByTagName`,
  (which does recursive search) but without recursive search
  (nested tags are not supported in histograms files).

  Note: This generator stops going deeper in the tree when it detects
  that there are elements with given tag.

  Args:
    root: XML dom tree.
    tag: Element's tag name.
    depth: Defines how deep in the tree function should search for a match.

  Yields:
    xml.dom.minidom.Node: Element matching criteria.

  """
  if depth == 0 and root.nodeType == _ELEMENT_NODE and root.tagName == tag:
    yield root
    return

  had_tag = False

  skipped = 0

  for child in root.childNodes:
    if child.nodeType == _ELEMENT_NODE and child.tagName == tag:
      had_tag = True
      yield child
    else:
      skipped += 1

  depth -= 1

  if not had_tag and depth != 0:
    for child in root.childNodes:
      for match in IterElementsWithTag(child, tag, depth):
        yield match
