# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility file for pretty printing xml file.

The function PrettyPrintXml will be used for formatting both histograms.xml
and actions.xml.
"""

import sys
import logging
import textwrap
from xml.dom import minidom
import xml.etree.ElementTree as ET

import etree_util

WRAP_COLUMN = 80


class Error(Exception):
  pass


def LastLineLength(s):
  """Returns the length of the last line in s.

  Args:
    s: A multi-line string, including newlines.

  Returns:
    The length of the last line in s, in characters.
  """
  if s.rfind('\n') == -1:
    return len(s)
  return len(s) - s.rfind('\n') - len('\n')


def XmlEscape(s):
  """Returns escaped string for the given string |s|."""
  s = s.replace('&', '&amp;').replace('<', '&lt;')
  s = s.replace('\"', '&quot;').replace('>', '&gt;')
  return s


def SplitParagraphs(text):
  """Split a block of text into paragraphs.

  Args:
    text: The text to split.

  Returns:
    A list of paragraphs as strings.
  """
  text = textwrap.dedent(text.strip('\n'))
  lines = text.split('\n')
  # Split the text into paragraphs at blank line boundaries.
  paragraphs = [[]]
  for l in lines:
    if paragraphs[-1] and not l.strip():
      paragraphs.append([])
    else:
      # Replace runs of repeated whitespace with a single space.
      transformed_line = ' '.join(l.split())
      paragraphs[-1].append(transformed_line)
  # Remove trailing empty paragraph if present.
  if paragraphs and not paragraphs[-1]:
    paragraphs = paragraphs[:-1]
  return ['\n'.join(p) for p in paragraphs]


class XmlStyle(object):
  """A class that stores all style specification for an output xml file."""

  def __init__(self, attribute_order, required_attributes,
               tags_that_have_extra_newline, tags_that_dont_indent,
               tags_that_allow_single_line, tags_alphabetization_rules):
    self.attribute_order = attribute_order
    self.required_attributes = required_attributes
    self.tags_that_have_extra_newline = tags_that_have_extra_newline
    self.tags_that_dont_indent = tags_that_dont_indent
    self.tags_that_allow_single_line = tags_that_allow_single_line
    self.tags_alphabetization_rules = tags_alphabetization_rules

    self.wrapper = textwrap.TextWrapper()
    self.wrapper.break_on_hyphens = False
    self.wrapper.break_long_words = False
    self.wrapper.width = WRAP_COLUMN

  def PrettyPrintXml(self, tree):
    # If it's not an ElementTree instance, we assume it's minidom.
    if not isinstance(tree, ET.Element):
      assert isinstance(tree, minidom.Document)
      return self._PrettyPrintMinidom(tree)

    tree = self._TransformByAlphabetizing(tree)
    tree = self.PrettyPrintElementTreeNode(tree)
    return tree

  def _PrettyPrintMinidom(self, doc):
    """Transforms minidom to ElementTree before pretty printing it."""
    raw_xml = doc.toxml()

    # minidom prepends a document type, so remove it.
    raw_xml = raw_xml.replace(minidom.Document().toxml(), '')

    etree_root = etree_util.ParseXMLString(raw_xml)
    top_content = etree_util.GetTopLevelContent(raw_xml)

    # Add newlines between top-level comments.
    top_content = top_content.replace('--><!--', '-->\n\n<!--')

    formatted_xml = self.PrettyPrintXml(etree_root)
    return top_content + formatted_xml


  def _TransformByAlphabetizing(self, node):
    """Transform the given XML by alphabetizing nodes.

    Args:
      node: The elementtree node to transform.

    Returns:
      The elementtree node, with children appropriately alphabetized. Note that
      the transformation is done in-place, i.e. the original tree is modified
      directly.
    """
    # Element node with a tag name that we alphabetize the children of?
    alpha_rules = self.tags_alphabetization_rules
    if node.tag in alpha_rules:
      # Put subnodes in a list of node, key pairs to allow for custom sorting.
      subtags = {}
      for index, (subtag, key_function) in enumerate(alpha_rules[node.tag]):
        subtags[subtag] = (index, key_function)

      subnodes = []
      sort_key = -1
      pending_node_indices = []
      for c in node:
        if c.tag in subtags:
          subtag_sort_index, key_function = subtags[c.tag]
          sort_key = (subtag_sort_index, key_function(c))
          # Replace sort keys for delayed nodes.
          for idx in pending_node_indices:
            subnodes[idx][1] = sort_key
          pending_node_indices = []
        else:
          # Subnodes that we don't want to rearrange use the next node's key,
          # so they stay in the same relative position.
          # Therefore we delay setting key until the next node is found.
          pending_node_indices.append(len(subnodes))
        subnodes.append([c, sort_key])

      # Sort the subnode list.
      subnodes.sort(key=lambda pair: pair[1])

      # Remove the existing nodes
      for child in list(node):
        node.remove(child)

      # Re-add the sorted subnodes, transforming each recursively.
      for (c, _) in subnodes:
        node.append(self._TransformByAlphabetizing(c))
      return node

    # Recursively handle other element nodes and other node types.
    for c in node:
      self._TransformByAlphabetizing(c)
    return node

  def _PrettyPrintText(self, text, indent):
    """Pretty print an element."""
    if not text.strip():
      return ""

    self.wrapper.initial_indent = ' ' * indent
    self.wrapper.subsequent_indent = ' ' * indent
    escaped_text = XmlEscape(text)
    paragraphs = SplitParagraphs(escaped_text)

    # Wrap each paragraph and separate with two newlines.
    return '\n\n'.join(self.wrapper.fill(p) for p in paragraphs)

  def _PrettyPrintElement(self, node, indent):
    # Check if tag name is valid.
    if node.tag not in self.attribute_order:
      logging.error('Unrecognized tag "%s"', node.tag)
      raise Error('Unrecognized tag "%s"' % node.tag)

    # Newlines.
    newlines_after_open, newlines_before_close, newlines_after_close = (
        self.tags_that_have_extra_newline.get(node.tag, (1, 1, 0)))
    # Open the tag.
    s = ' ' * indent + '<' + node.tag

    # Calculate how much space to allow for the '>' or '/>'.
    closing_chars = 2
    if len(node) or node.text:
      closing_chars = 1

    attributes = node.keys()
    required_attributes = [attribute for attribute in self.required_attributes
                           if attribute in self.attribute_order[node.tag]]
    missing_attributes = [attribute for attribute in required_attributes
                          if attribute not in attributes]

    for attribute in missing_attributes:
      logging.error(
          'Missing attribute "%s" in tag "%s"', attribute, node.tag)
    if missing_attributes:
      missing_attributes_str = (
          ', '.join('"%s"' % attribute for attribute in missing_attributes))
      present_attributes = [
          ' {0}="{1}"'.format(name, value)
          for name, value in node.items()]
      node_str = '<{0}{1}>'.format(node.tag, ''.join(present_attributes))
      raise Error(
          'Missing attributes {0} in tag "{1}"'.format(
              missing_attributes_str, node_str))

    # Pretty-print the attributes.
    if attributes:
      # Reorder the attributes.
      unrecognized_attributes = [
          a for a in attributes if a not in self.attribute_order[node.tag]
      ]
      attributes = [
          a for a in self.attribute_order[node.tag] if a in attributes
      ]

      for a in unrecognized_attributes:
        logging.error('Unrecognized attribute "%s" in tag "%s"', a, node.tag)
      if unrecognized_attributes:
        raise Error('Unrecognized attributes {0} in tag "{1}"'.format(
            ', '.join('"{0}"'.format(a) for a in unrecognized_attributes),
            node.tag))

      for a in attributes:
        value = XmlEscape(node.get(a))
        # Replace sequences of whitespace with single spaces.
        words = value.split()
        a_str = ' %s="%s"' % (a, ' '.join(words))
        # Start a new line if the attribute will make this line too long.
        if LastLineLength(s) + len(a_str) + closing_chars > WRAP_COLUMN:
          s += '\n' + ' ' * (indent + 3)
        # Output everything up to the first quote.
        s += ' %s="' % (a)
        value_indent_level = LastLineLength(s)
        # Output one word at a time, splitting to the next line where
        # necessary.
        column = value_indent_level
        for i, word in enumerate(words):
          # This is slightly too conservative since not every word will be
          # followed by the closing characters...
          if i > 0 and (column + len(word) + 1 + closing_chars > WRAP_COLUMN):
            s = s.rstrip()  # remove any trailing whitespace
            s += '\n' + ' ' * value_indent_level
            column = value_indent_level
          s += word + ' '
          column += len(word) + 1
        s = s.rstrip()  # remove any trailing whitespace
        s += '"'
      s = s.rstrip()  # remove any trailing whitespace

    # Pretty-print the child nodes.
    if len(node) > 0 or node.text:  # pylint: disable=g-explicit-length-test
      s += '>'
      # Calculate the new indent level for child nodes.
      new_indent = indent
      if node.tag not in self.tags_that_dont_indent:
        new_indent += 2

      children = []
      for c in node:
        children.append(c)

      # Recursively pretty-print the child nodes.
      child_nodes = []
      if node.text:
        formatted_text = self._PrettyPrintText(node.text, new_indent)
        if formatted_text:
          child_nodes.append(formatted_text)

      for child in node:
        child_output = self.PrettyPrintElementTreeNode(child, indent=new_indent)
        if child_output.strip():
          child_nodes.append(child_output)

        if child.tail:
          tail_text = self._PrettyPrintText(child.tail, new_indent)
          if tail_text:
            child_nodes.append(tail_text)

      # Determine whether we can fit the entire node on a single line.
      close_tag = '</%s>' % node.tag
      space_left = WRAP_COLUMN - LastLineLength(s) - len(close_tag)
      if (node.tag in self.tags_that_allow_single_line and
          len(child_nodes) == 1 and len(child_nodes[0].strip()) <= space_left):
        s += child_nodes[0].strip()
      else:
        s += '\n' * newlines_after_open + '\n'.join(child_nodes)
        s += '\n' * newlines_before_close + ' ' * indent
      s += close_tag
    else:
      s += '/>'
    s += '\n' * newlines_after_close
    return s

  def PrettyPrintElementTreeNode(self, node, indent=0):
    """Pretty-prints the given XML node at the given indent level.

    Args:
      node: The ElementTree node to pretty-print.
      indent: The current indent level.

    Returns:
      The pretty-printed string (including embedded newlines).

    Raises:
      Error: if the XML has unknown tags or attributes.
    """
    # Handle comment nodes.
    if node.tag is ET.Comment:
      return '<!--%s-->\n' % node.text

    # Handle element nodes.
    return self._PrettyPrintElement(node, indent)
