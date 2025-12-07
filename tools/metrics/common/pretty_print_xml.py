# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility file for pretty printing xml file.

The function PrettyPrintXml will be used for formatting both histograms.xml
and actions.xml.
"""
import dataclasses
import itertools
import logging
import sys
import textwrap
import collections

from abc import abstractmethod
from typing import (Any, Iterable, Mapping, Sequence, Protocol, List, Tuple,
                    Optional, OrderedDict)
from xml.dom import minidom

import xml.etree.ElementTree as ET

import etree_util

WRAP_COLUMN = 80

_IFTTT_START_TEXT = 'LINT.IfChange'
_IFTTT_END_TEXT = 'LINT.ThenChange'
_IFTTT_START_PRIORITY = -1
_IFTTT_END_PRIORITY = sys.maxsize
_DEFAULT_END_NODE_PRIORITY = sys.maxsize

_COMMENTS_ROOT_ID = 0
_PLACEHOLDER_COMMENT_ID = -1

# Type representing Level IDs in Comments Tree.
LevelIDsType = Tuple[int, ...]


class Comparable(Protocol):

  @abstractmethod
  def __lt__(self, other: Any) -> bool:
    pass


SortKey = Tuple[Comparable, Comparable]


@dataclasses.dataclass
class _CommentsTree:
  """Represents a tree of comments used to enforce the IfThisThenThat rules.

  We transform the XML tree into a comments tree in order to be able to
  correctly sort the nodes and make sure that the contents of the IFTTT
  comments are not accidentally changed. Internal nodes in the comments tree
  represent the block that are created by the IFTTT comments.

  Following XML file

  <enums>
  <-- LINT.IfChange(A) -->
  <enum name="TestEnum">
    <-- LINT.IfChange(B) -->
    <int value="0" label="A"/>
    <-- LINT.ThenChange(B) -->
  </enum>
  <-- LINT.ThenChange(A) -->
  </enums>

  is divided into the IFTTT blocks (indentation marks the new IFTTT block):

  <enums>
    <-- LINT.IfChange(A) -->
    <enum name="TestEnum">
      <-- LINT.IfChange(B) -->
      <int value="0" label="A"/>
      <-- LINT.ThenChange(B) -->
    <-- LINT.ThenChange(A) -->
    </enum>
  </enums>

  and can be represented as the comments tree:

  _CommentsTree(id=0, children=[
    _CommentsTree(id=-1, xml_element=<enums>)
    _CommentsTree(id=1, children=[
      _CommentsTree(id=-1, xml_element=<-- LINT.IfChange(A) -->)
      _CommentsTree(id=-1, xml_element=<enum name="TestEnum">)
      _CommentsTree(id=2, children=[
        _CommentsTree(id=-1, xml_element=<-- LINT.IfChange(B) -->)
        _CommentsTree(id=-1, xml_element=<int value="0" label="A"/>)
        _CommentsTree(id=-1, xml_element=<-- LINT.ThenChange(B) -->)
      ])
      _CommentsTree(id=-1, xml_element=<-- LINT.ThenChange(A) -->)
    ])
  ]),


  Attributes:
    id: The id of the comment block. This id is used to distinguish
      the internal nodes, the leaf nodes have the id of -1.
    children: The children of the node.
    sort_key: The key that should be used to sort the children of this node.
    xml_element: The XML element that this node represents. This is None for
      internal nodes, ant not None for leaf nodes.
  """
  id: int
  children: List['_CommentsTree'] = dataclasses.field(default_factory=list)
  sort_key: Optional[SortKey] = None
  xml_element: Optional[ET.Element] = None

  def GetSortKey(self) -> SortKey:
    if self.sort_key:
      return self.sort_key

    # Initially the sort key is only set for the leaf nodes, therefore we're
    # searching for the sort key in the children list.
    # If the children nodes get sorted in ascending order, we want to use
    # the key of the first child to represent the sort_key of parent and
    # we can find it using the min function.
    self.sort_key = min(
        child.GetSortKey() for child in self.children
        # We want to sort using the sort key from the nodes with real values,
        # not the IFTTT comment nodes.
        if child.GetSortKey()[0] != _IFTTT_START_PRIORITY)
    return self.sort_key

  def __lt__(self, other: '_CommentsTree') -> bool:
    return self.GetSortKey() < other.GetSortKey()

  def __iter__(self) -> Iterable[ET.Element]:
    """Returns iterator over the values in the comments tree in the preorder
    traversal.
    """
    if self.xml_element is not None:
      yield self.xml_element

    for child in self.children:
      yield from child

  def Sort(self, prefix_level_ids: Sequence[int] = ()) -> None:
    """Sorts the children of this node by their sort key. Ensures the ones
    matching the given level ids are kept at the top of the children list
    regardless of their sort key.

    If in the source XMLtree the IFTTT block started before the parent node
    we have to make sure that all the nodes within this block are first in the
    children list, otherwise we might move the end comment of the IFTTT block.
    Consider example:
     <-- Lint.IfChange -->
     <enum name="enum">
     <int value="1" label="B"/>
      <-- Lint.ThenChange(...) -->
     <int value="0" label="A"/>
     </enum>

    Since the block starts before the node "enum", we need to keep
    the label "B" above the label "A".

    Args:
      prefix_level_ids: The level ids that should be kept at the top of the
      children list.
    """
    children_to_skip = []
    children_to_sort = []
    for child in self.children:
      if child.id in prefix_level_ids:
        children_to_skip.append(child)
      else:
        children_to_sort.append(child)

    self.children = children_to_skip + sorted(children_to_sort)

    # Recursively sort the children.
    for child in self.children:
      child.Sort(prefix_level_ids)


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


def _NodeTextStartsWith(node: ET.Element, text: str) -> bool:
  """Returns whether the given node's text starts with the given text."""
  return node.text and node.text.strip().lower().startswith(text.lower())


def _IsIFTTTBlockStart(node: ET.Element) -> bool:
  """Returns whether the given node is the start of an IFTTT block."""
  return _NodeTextStartsWith(node, _IFTTT_START_TEXT)


def _IsIFTTTBlockEnd(node: ET.Element) -> bool:
  """Returns whether the given node is the end of an IFTTT block."""
  return _NodeTextStartsWith(node, _IFTTT_END_TEXT)


def _IterNodes(root: ET.Element) -> Iterable[ET.Element]:
  """Returns an iterator over all the nodes in the given tree in the
    preorder traversal.
    """
  yield root
  for c in root:
    yield from _IterNodes(c)


def _CalculateIFTTTCommentsLevels(
    root: ET.Element) -> Mapping[ET.Element, LevelIDsType]:
  """Returns a mapping of nodes to the level ids of the IFTTT comments.

    IFTTT comments are used to enforce that certain changes are made in
    multiple files. The comments are used to specify the files that should
    be changed. Since the IFTTT comments can be nested, we need to keep
    track of the level of the comment in order to correctly sort the nodes and
    make sure that the contents of the IFTTT are not accidentally changed.

    When new IFTTT block starts, it gets a new unique id. Level id of the
    block is a tuple of all the ids of the parent IFTTT blocks.

    Args:
      root: The root node of the tree to calculate the levels for.
    Returns:
      A mapping of nodes to the level ids of the IFTTT comments.
    """
  levels = {}
  id_generator = itertools.count()
  curr_id = (next(id_generator), )
  for node in _IterNodes(root):
    if _IsIFTTTBlockStart(node):
      curr_id = curr_id + (next(id_generator), )

    levels[node] = curr_id

    if _IsIFTTTBlockEnd(node):
      curr_id = curr_id[:-1]
  return levels


def _CreateCommentsTree(
    subnodes_map: Mapping[ET.Element, SortKey],
    level_ids: Mapping[ET.Element, LevelIDsType]) -> _CommentsTree:
  """Creates a comments tree from the given subnodes map and level ids.

    Args:
      subnodes_map: A mapping of subnode to sort key.
      level_ids: A mapping of subnode to level ids. Level id of the IFTTT
      block is a tuple of all the ids of the parent IFTTT blocks.
    Returns:
      Root of the created comments tree.
    """
  comments_root = _CommentsTree(id=_COMMENTS_ROOT_ID)
  comments_tree_dict = {_COMMENTS_ROOT_ID: comments_root}

  for node, sort_key in subnodes_map.items():
    # Search for the comments node that corresponds to the level of
    # the current subnode.
    comments_parent = comments_root
    for level_id in level_ids[node]:
      # Ensure intermediate nodes were previously created and are present
      # in the comments_tree_dict.
      if level_id not in comments_tree_dict:
        comments_tree_node = _CommentsTree(id=level_id)
        comments_tree_dict[level_id] = comments_tree_node
        comments_parent.children.append(comments_tree_node)
      comments_parent = comments_tree_dict[level_id]

    comments_node = _CommentsTree(id=_PLACEHOLDER_COMMENT_ID,
                                  xml_element=node,
                                  sort_key=sort_key)
    comments_parent.children.append(comments_node)

  return comments_root


def _SortSubnodesByLevelIds(
    parent_node: ET.Element,
    subnodes_map: OrderedDict[ET.Element, SortKey],
    level_ids: Mapping[ET.Element, LevelIDsType],
) -> Iterable[ET.Element]:
  """Sorts the subnodes of the given parent node by the level ids in the IFTTT
    comments tree.
    Note that new iterator is created, the original subnodes are not modified.

    Args:
      parent_node: The parent node to sort the subnodes of.
      subnodes_map: A mapping of subnode to sort key.
      level_ids: A mapping of subnode to level ids. Level id of the IFTTT
      block is a tuple of all the ids of the parent IFTTT blocks.
    Returns:
      An iterable of subnodes sorted by level ids.
    """
  comments_root = _CreateCommentsTree(subnodes_map, level_ids)
  comments_root.Sort(level_ids[parent_node])
  return comments_root


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

  def _TransformByAlphabetizing(
      self,
      node: ET.Element,
      level_ids: Optional[Mapping[ET.Element,
                                  LevelIDsType]] = None) -> ET.Element:
    """Transform the given XML by alphabetizing nodes.

    Args:
      node: The elementtree node to transform.
      level_ids: A mapping of elementtree nodes to IFTTT level ids. Level id of
      the IFTTT block is a tuple of all the ids of the parent IFTTT blocks.

    Returns:
      The elementtree node, with children appropriately alphabetized. Note that
      the transformation is done in-place, i.e. the original tree is modified
      directly.
    """
    # Element node with a tag name that we alphabetize the children of?
    alpha_rules = self.tags_alphabetization_rules
    if level_ids is None:
      level_ids = _CalculateIFTTTCommentsLevels(node)

    if node.tag in alpha_rules:
      # Put subnodes in a list of node, key pairs to allow for custom sorting.
      subtags = {}
      for index, (subtag, key_function) in enumerate(alpha_rules[node.tag]):
        subtags[subtag] = (index, key_function)

      # Map from the subnode to its sort key.
      subnodes_map: OrderedDict[ET.Element, SortKey] = collections.OrderedDict()
      # List of nodes whose sort key has not been found yet (their sort_key
      # will be set when the first suitable node is found).
      pending_nodes: List[ET.Element] = []
      for child in node:
        if child.tag in subtags:
          subtag_sort_index, key_function = subtags[child.tag]
          sort_key = (subtag_sort_index, key_function(child))
          # Replace sort keys for delayed nodes.
          for pending_node in pending_nodes:
            subnodes_map[pending_node] = sort_key
          pending_nodes = []
        elif _IsIFTTTBlockStart(child):
          sort_key = (_IFTTT_START_PRIORITY, -1)
        elif _IsIFTTTBlockEnd(child):
          sort_key = (_IFTTT_END_PRIORITY, -1)
        else:
          # Subnodes that we don't want to rearrange use the next node's key,
          # so they stay in the same relative position.
          # Therefore we delay setting key until the next node is found.
          pending_nodes.append(child)
          # Set sort key to dummy value that will be overwritten in the future.
          sort_key = None
        subnodes_map[child] = sort_key

      # Set the sort key for any leftover pending nodes.
      for pending_node in pending_nodes:
        subnodes_map[pending_node] = (_DEFAULT_END_NODE_PRIORITY, -1)

      # Sort the subnode list.
      subnodes = _SortSubnodesByLevelIds(node, subnodes_map, level_ids)

      # Remove the existing nodes
      for child in list(node):
        node.remove(child)

      # Re-add the sorted subnodes, transforming each recursively.
      for child in subnodes:
        node.append(self._TransformByAlphabetizing(child, level_ids))
      return node

    # Recursively handle other element nodes and other node types.
    for child in node:
      self._TransformByAlphabetizing(child, level_ids)
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
    missing_attributes = [
        attribute for attribute in self.required_attributes[node.tag]
        if attribute not in attributes
    ]

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
