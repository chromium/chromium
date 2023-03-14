# -*- coding: utf-8 -*-
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


def error(elem, msg):
  """Raise a nicely formatted error with some context."""
  name = elem.attrib.get("name", None)
  name = name + " " if name else ""
  msg = "Structured metrics error, {} node {}: {}.".format(elem.tag, name, msg)
  raise ValueError(msg)


def get_attr(elem, tag, regex=None):
  """Get an attribute.

  Error if it is missing, optionally error if it doesn't match the provided
  regex.
  """
  attr = elem.attrib.get(tag, None)
  if not attr:
    error(elem, "missing attribute '{}'".format(tag))
  if regex and not re.match(regex, attr):
    error(elem, ("has '{}' attribute '{}' which does "
                 "not match regex '{}'").format(tag, attr, regex))
  return attr


def get_optional_attr(elem, tag, regex=None):
  """Get an attribute.

  Returns None if it doesn't exist.
  """
  attr = elem.attrib.get(tag)
  if not attr:
    return None
  if regex and not re.match(regex, attr):
    error(elem, ("has '{}' attribute '{}' which does "
                 "not match regex '{}'").format(tag, attr, regex))
  return attr


def get_compound_children(elem, tag, allow_missing_children=False):
  """Get all child nodes of `elem` with tag `tag`.

  Error if none exist, or a child is not a compound node.
  """
  children = elem.findall(tag)
  if not children and not allow_missing_children:
    error(elem, "missing node '{}'".format(tag))
  for child in children:
    if child.text and child.text.strip():
      error(child, "contains text, but shouldn't")
  return children


def get_compound_child(elem, tag):
  """Get the child of `elem` with tag `tag`.

  Error if there isn't exactly one matching child, or it isn't compound.
  """
  children = elem.findall(tag)
  if len(children) != 1:
    error(elem, "needs exactly one '{}' node".format(tag))
  return children[0]


def get_text_children(elem, tag, regex=None):
  """Get the text of all child nodes of `elem` with tag `tag`.

  Error if none exist, or a child is not a text node. Optionally ensure the
  text matches `regex`.
  """
  children = elem.findall(tag)
  if not children:
    error(elem, "missing node '{}'".format(tag))

  result = []
  for child in children:
    check_attributes(child, set())
    check_children(child, set())
    text = child.text.strip()
    if not text:
      error(elem, "missing text in '{}'".format(tag))
    if regex and not re.match(regex, text):
      error(elem, ("has '{}' node '{}' which does "
                   "not match regex '{}'").format(tag, text, regex))
    result.append(text)
  return result


def get_text_child(elem, tag, regex=None):
  """Get the text of the child of `elem` with tag `tag`.

  Error if there isn't exactly one matching child, or it isn't a text node.
  Optionally ensure the text matches `regex`.
  """
  result = get_text_children(elem, tag, regex)
  if len(result) != 1:
    error(elem, "needs exactly one '{}' node".format(tag))
  return result[0]


def check_attributes(elem, expected_attrs, optional_attrs=None):
  """Ensure `elem` has no attributes except those in `expected_attrs`."""
  actual_attrs = set(elem.attrib.keys())
  unexpected_attrs = actual_attrs - set(expected_attrs)
  if optional_attrs:
    unexpected_attrs = unexpected_attrs - set(optional_attrs)
  if unexpected_attrs:
    attrs = " ".join(unexpected_attrs)
    error(elem, "has unexpected attributes: " + attrs)


def check_children(elem, expected_children):
  """Ensure all children in `expected_children` are in `elem`."""
  actual_children = {child.tag for child in elem}
  unexpected_children = set(expected_children) - actual_children
  if unexpected_children:
    children = " ".join(unexpected_children)
    error(elem, "is missing nodes: " + children)


def check_child_names_unique(elem, tag):
  """Ensure uniqueness of the 'name' of all children of `elem` with `tag`."""
  names = [child.attrib.get("name", None) for child in elem if child.tag == tag]
  name_counts = collections.Counter(names)
  has_duplicates = any(c > 1 for c in name_counts.values())
  if has_duplicates:
    error(elem, "has {} nodes with duplicate names".format(tag))
