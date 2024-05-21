# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for parsing structured.xml.

Functions in this module raise an error if constraints on the format of the
structured.xml file are not met.

Functions use the concept of 'compound' and 'text' XML nodes.
 - compound nodes can have attributes and child nodes, but no text
 - text nodes can have text, but no attributes or child nodes
"""

import collections
import re
from typing import List, Optional, Set
import xml.etree.ElementTree as ET


BOOLEAN_REGEX = r"(?i)(true|false|)$"


def error(elem: ET.Element, msg: str) -> None:
  """Raise a nicely formatted error with some context."""
  name = elem.attrib.get("name", None)
  name = name + " " if name else ""
  msg = f"Structured metrics error, {elem.tag} node {name}: {msg}."
  raise ValueError(msg)


def get_attr(elem: ET.Element,
             tag: str,
             regex: Optional[str] = None) -> Optional[str]:
  """Get an attribute.

    Error if it is missing, optionally error if it doesn't match the provided
    regex.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.
      regex: Optional regex to match the value of the attribute.

    Returns:
      The value of the attribute.

    Raises:
      ValueError: The attribute specified is missing or doesn't match provided
      regex.
    """
  attr = elem.attrib.get(tag, None)
  if not attr:
    error(elem, f"missing attribute '{tag}'")
  if regex and not re.fullmatch(regex, attr):
    error(
        elem,
        (f"has '{tag}' attribute '{attr}' which does "
         "not match regex '{regex}'"),
    )
  return attr


def get_text(elem: ET.Element, regex: Optional[str] = None) -> str:
  """ Get the text of an element.

    Error if it is missing text, optionally error if it doesn't match the
    provided regex.

    Args:
      elem: structured.xml file element tree.
      regex: Optional regex to match the value of the attribute.

    Returns:
      The text value of the element.

    Raises:
      ValueError: The attribute specified is missing or doesn't match provided
      regex.
  """
  if not elem.text:
    error(elem, "doesn't containt text")

  text = elem.text.strip()
  if regex and not re.match(regex, text):
    error(elem, ("text '{}' does not match regex '{}'").format(text, regex))

  return text


def get_optional_attr(elem: ET.Element, tag: str, regex: str = None) -> str:
  """Get an attribute.

    Returns None if it doesn't exist.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.
      regex: Optional regex to match the value of the attribute.

    Returns:
      The value of the attribute, or None if it doesn't exist.

    Raises:
      ValueError: The attribute specified exists but doesn't match provided
      regex.
    """
  attr = elem.attrib.get(tag)
  if not attr:
    return None
  if regex and not re.fullmatch(regex, attr):
    error(
        elem,
        (f"has '{tag}' attribute '{attr}' which does "
         "not match regex '{regex}'"),
    )
  return attr


def get_optional_attr_list(elem: ET.Element,
                           tag: str,
                           regex: str = None) -> List[str]:
  """Get an attribute that is a comma separated list.

  Returns None if it doesn't exist.

  Args:
    elem: structured.xml file element tree.
    tag: Name of the tag/attribute to find.
    regex: Optional regex to match the value of the attribute's items.

  Returns:
    The attribute's value as a list of strings, or None if it doesn't exist.

  Raises:
    ValueError: The attribute specified exists but one of its items doesn't
    match the provided regex.
  """
  attr = elem.attrib.get(tag)
  if not attr:
    return None
  attr_list = attr.split(",")
  attributes = []
  for attr_item in attr_list:
    if regex and not re.fullmatch(regex, attr_item):
      error(
          elem,
          (f"has '{tag}' that contains attribute"
           " '{attr_item}' which does "
           "not match regex '{regex}'"),
      )
    attributes.append(attr_item)
  return attributes


def get_compound_children(elem: ET.Element,
                          tag: str,
                          allow_missing_children: bool = False,
                          allow_text: bool = False) -> List[ET.Element]:
  """Get all child nodes of `elem` with tag `tag`.

    Error if none exist, or a child is not a compound node.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag to find.
      allow_missing_children: If True does not raise an
        error when there are no children.
      allow_text: If true does not raise an error when child contain text.

    Returns:
      All child nodes of 'elem' with the specified tag 'tag'.

    Raises:
      ValueError: child node of 'elem' with tag 'tag' is
        not a compound node, or does not have children nodes
        when `allow_missing_children` is False.
    """
  children = elem.findall(tag)
  if not children and not allow_missing_children:
    error(elem, f"missing node '{tag}'")

  if not allow_text:
    for child in children:
      if child.text and child.text.strip():
        error(child, "contains text, but shouldn't")
  return children


def get_compound_child(elem: ET.Element, tag: str) -> ET.Element:
  """Get the child of `elem` with tag `tag`.

    Error if there isn't exactly one matching child, or it isn't compound.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.

    Returns:
      The child node.

    Raises:
      ValueError: If there isn't exactly one matching child node.
    """
  children = elem.findall(tag)
  if len(children) != 1:
    error(elem, f"needs exactly one '{tag}' node")
  return children[0]


def get_text_children(elem: ET.Element,
                      tag: str,
                      regex: Optional[str] = None) -> List[Optional[str]]:
  """Get the text of all child nodes of `elem` with tag `tag`.

    Error if none exist, or a child is not a text node. Optionally ensure the
    text matches `regex`.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.
      regex: Optional regex to match the value of the attribute.

    Returns:
      Text of all child nodes of 'elem' with tag 'tag'.

    Raises:
      ValueError: The attribute specified is missing or doesn't match provided
      regex.
    """
  children = elem.findall(tag)
  if not children:
    error(elem, f"missing node '{tag}'")

  result = []
  for child in children:
    check_attributes(child, set())
    check_children(child, set())
    text = child.text.strip() if child.text else None
    if not text:
      error(elem, f"missing text in '{tag}'")
    if regex and not re.fullmatch(regex, text):
      error(
          elem,
          (f"has '{tag}' node '{text}' which does "
           "not match regex '{regex}'"),
      )
    result.append(text)
  return result


def get_text_child(elem: ET.Element,
                   tag: str,
                   regex: Optional[str] = None) -> Optional[str]:
  """Get the text of the child of `elem` with tag `tag`.

    Error if there isn't exactly one matching child, or it isn't a text node.
    Optionally ensure the text matches `regex`.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.
      regex: Optional regex to match the value of the attribute.

    Returns:
      Text of the child of 'elem' with tag 'tag'.

    Raises:
      ValueError: There isn't exactly one matching child that is a text node.
    """
  result = get_text_children(elem, tag, regex)
  if len(result) != 1:
    error(elem, f"needs exactly one '{tag}' node")
  return result[0]


def check_attributes(
    elem: ET.Element,
    expected_attrs: Set[str],
    optional_attrs: Optional[Set[str]] = None,
) -> None:
  """Ensure `elem` has no attributes except those in `expected_attrs`.

    Args:
      elem: structured.xml file element tree.
      expected_attrs: set of the expected attribute names.
      optional_attrs: Optional set of attributes that are optional.

    Returns:
      None

    Raises:
      ValueError: Unexpected attributes exist for 'elem'.
    """
  actual_attrs = set(elem.attrib.keys())
  unexpected_attrs = actual_attrs - set(expected_attrs)
  if optional_attrs:
    unexpected_attrs = unexpected_attrs - set(optional_attrs)
  if unexpected_attrs:
    attrs = " ".join(unexpected_attrs)
    error(elem, "has unexpected attributes: " + attrs)


def check_children(elem: ET.Element,
                   expected_children: Set[str],
                   optional_children: Optional[Set[str]] = None) -> None:
  """Ensure all children in `expected_children` are in `elem`.

    Args:
      elem: structured.xml file element tree.
      expected_children: set of expected children by tag name.
      optional_children: set of children by tag name that may or may not be
                         present.

    Returns:
      None

    Raises:
      ValueError: Not all expected children exist for 'elem'.
    """
  actual_children = {child.tag for child in elem}
  unexpected_children = set(expected_children) - actual_children

  if optional_children:
    unexpected_children = unexpected_children - set(optional_children)

  if unexpected_children:
    children = " ".join(unexpected_children)
    error(elem, "is missing nodes: " + children)


def get_boolean_attr(elem: ET.Element, attr_name: str) -> bool:
  """Get the Boolean value of the specified attribute 'attr_name'.

    Args:
      elem: structured.xml file element tree.
      attr_name: Name of the attribute to find.

    Returns:
      Boolean of the specified attribute, False if the attribute does not exist.
    """
  maybe_attr = get_optional_attr(elem, attr_name, BOOLEAN_REGEX)
  if maybe_attr:
    return maybe_attr.lower() == "true"
  return False


def check_child_names_unique(elem: ET.Element, tag: str) -> None:
  """Ensure uniqueness of the 'name' of all children of `elem` with `tag`.

    Args:
      elem: structured.xml file element tree.
      tag: Name of the tag/attribute to find.

    Returns:
      None

    Raises:
      ValueError: children of 'elem' with 'tag' are not unique.
    """
  names = [child.attrib.get("name", None) for child in elem if child.tag == tag]
  check_names_unique(elem, names, tag)


def check_names_unique(elem: ET.Element, names: List[str], tag: str) -> None:
  """Ensures that the names provided are unique.

    Args:
      elem: structured.xml file element tree.
      names: The list of names.
      tag: Name of the tag/attribute to find.

    Returns:
      None

    Raises:
      ValueError: children of 'elem' with 'tag' are not unique.

  """

  name_counts = collections.Counter(names)
  has_duplicates = any(c > 1 for c in name_counts.values())
  if has_duplicates:
    error(elem, f"has {tag} nodes with duplicate names")
