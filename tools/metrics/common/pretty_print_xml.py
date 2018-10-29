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
import xml.dom.minidom

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
  if s.rfind('\n') == -1: return len(s)
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

  def PrettyPrintXml(self, tree):
    tree = self._TransformByAlphabetizing(tree)
    tree = self.PrettyPrintNode(tree)
    return tree

  def _UnsafeAppendChild(self, parent, child):
    """Append child to parent's list of children.

    It ignores the possibility that the child is already in another node's
    childNodes list.  Requires that the previous parent of child is discarded
    (to avoid non-tree DOM graphs). This can provide a significant speedup as
    O(n^2) operations are removed (in particular, each child insertion avoids
    the need to traverse the old parent's entire list of children).

    Args:
      parent: the parent node to be appended to.
      child: the child node to append to |parent| node.
    """
    child.parentNode = None
    parent.appendChild(child)
    child.parentNode = parent

  def _TransformByAlphabetizing(self, node):
    """Transform the given XML by alphabetizing nodes.

    Args:
      node: The minidom node to transform.

    Returns:
      The minidom node, with children appropriately alphabetized. Note that the
      transformation is done in-place, i.e. the original minidom tree is
      modified directly.
    """
    if node.nodeType != xml.dom.minidom.Node.ELEMENT_NODE:
      for c in node.childNodes:
        self._TransformByAlphabetizing(c)
      return node

    # Element node with a tag name that we alphabetize the children of?
    alpha_rules = self.tags_alphabetization_rules
    if node.tagName in alpha_rules:
      # Put subnodes in a list of node, key pairs to allow for custom sorting.
      subtags = {}
      for index, (subtag, key_function) in enumerate(alpha_rules[node.tagName]):
        subtags[subtag] = (index, key_function)

      subnodes = []
      sort_key = -1
      pending_node_indices = []
      for c in node.childNodes:
        if (c.nodeType == xml.dom.minidom.Node.ELEMENT_NODE and
            c.tagName in subtags):
          subtag_sort_index, key_function = subtags[c.tagName]
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

      # Re-add the subnodes, transforming each recursively.
      while node.firstChild:
        node.removeChild(node.firstChild)
      for (c, _) in subnodes:
        self._UnsafeAppendChild(node, self._TransformByAlphabetizing(c))
      return node

    # Recursively handle other element nodes and other node types.
    for c in node.childNodes:
      self._TransformByAlphabetizing(c)
    return node

  def PrettyPrintNode(self, node, indent=0):
    """Pretty-prints the given XML node at the given indent level.

    Args:
      node: The minidom node to pretty-print.
      indent: The current indent level.

    Returns:
      The pretty-printed string (including embedded newlines).

    Raises:
      Error: if the XML has unknown tags or attributes.
    """
    # Handle the top-level document node.
    if node.nodeType == xml.dom.minidom.Node.DOCUMENT_NODE:
      return '\n'.join([self.PrettyPrintNode(n) for n in node.childNodes])

    # Handle text nodes.
    if node.nodeType == xml.dom.minidom.Node.TEXT_NODE:
      # Wrap each paragraph in the text to fit in the 80 column limit.
      wrapper = textwrap.TextWrapper()
      wrapper.initial_indent = ' ' * indent
      wrapper.subsequent_indent = ' ' * indent
      wrapper.break_on_hyphens = False
      wrapper.break_long_words = False
      wrapper.width = WRAP_COLUMN
      text = XmlEscape(node.data)
      paragraphs = SplitParagraphs(text)
      # Wrap each paragraph and separate with two newlines.
      return '\n\n'.join(wrapper.fill(p) for p in paragraphs)

    # Handle element nodes.
    if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
      # Check if tag name is valid.
      if node.tagName not in self.attribute_order:
        logging.error('Unrecognized tag "%s"', node.tagName)
        raise Error('Unrecognized tag "%s"', node.tagName)

      # Newlines.
      newlines_after_open, newlines_before_close, newlines_after_close = (
          self.tags_that_have_extra_newline.get(node.tagName, (1, 1, 0)))
      # Open the tag.
      s = ' ' * indent + '<' + node.tagName

      # Calculate how much space to allow for the '>' or '/>'.
      closing_chars = 1
      if not node.childNodes:
        closing_chars = 2

      attributes = node.attributes.keys()
      required_attributes = [attribute for attribute in self.required_attributes
                             if attribute in self.attribute_order[node.tagName]]
      missing_attributes = [attribute for attribute in required_attributes
                            if attribute not in attributes]

      for attribute in missing_attributes:
        logging.error(
            'Missing attribute "%s" in tag "%s"', attribute, node.tagName)
      if missing_attributes:
        missing_attributes_str = (
            ', '.join('"%s"' % attribute for attribute in missing_attributes))
        present_attributes = [
            ' {0}="{1}"'.format(name, value)
            for name, value in node.attributes.items()]
        node_str = '<{0}{1}>'.format(node.tagName, ''.join(present_attributes))
        raise Error(
            'Missing attributes {0} in tag "{1}"'.format(
                missing_attributes_str, node_str))

      # Pretty-print the attributes.
      if attributes:
        # Reorder the attributes.
        unrecognized_attributes = (
            [a for a in attributes
             if a not in self.attribute_order[node.tagName]])
        attributes = [a for a in self.attribute_order[node.tagName]
                      if a in attributes]

        for a in unrecognized_attributes:
          logging.error(
              'Unrecognized attribute "%s" in tag "%s"', a, node.tagName)
        if unrecognized_attributes:
          raise Error(
              'Unrecognized attributes {0} in tag "{1}"'.format(
                  ', '.join('"{0}"'.format(a) for a in unrecognized_attributes),
                  node.tagName))

        for a in attributes:
          value = XmlEscape(node.attributes[a].value)
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
      if node.childNodes:
        s += '>'
        # Calculate the new indent level for child nodes.
        new_indent = indent
        if node.tagName not in self.tags_that_dont_indent:
          new_indent += 2
        child_nodes = node.childNodes

        # Recursively pretty-print the child nodes.
        child_nodes = [self.PrettyPrintNode(n, indent=new_indent)
                       for n in child_nodes]
        child_nodes = [c for c in child_nodes if c.strip()]

        # Determine whether we can fit the entire node on a single line.
        close_tag = '</%s>' % node.tagName
        space_left = WRAP_COLUMN - LastLineLength(s) - len(close_tag)
        if (node.tagName in self.tags_that_allow_single_line and
            len(child_nodes) == 1 and
            len(child_nodes[0].strip()) <= space_left):
          s += child_nodes[0].strip()
        else:
          s += '\n' * newlines_after_open + '\n'.join(child_nodes)
          s += '\n' * newlines_before_close + ' ' * indent
        s += close_tag
      else:
        s += '/>'
      s += '\n' * newlines_after_close
      return s

    # Handle comment nodes.
    if node.nodeType == xml.dom.minidom.Node.COMMENT_NODE:
      return '<!--%s-->\n' % node.data

    # Ignore other node types. This could be a processing instruction
    # (<? ... ?>) or cdata section (<![CDATA[...]]!>), neither of which are
    # legal in the histograms XML at present.
    logging.error('Ignoring unrecognized node data: %s', node.toxml())
    raise Error('Ignoring unrecognized node data: {0}'.format(node.toxml()))
