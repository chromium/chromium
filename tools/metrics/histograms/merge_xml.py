#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to merge multiple source xml files into a single histograms.xml."""

import argparse
import xml.dom.minidom

import expand_owners
import extract_histograms


def GetElementsByTagName(trees, tag):
  """Gets all elements with the specified tag from a set of DOM trees.

  Args:
    trees: A list of DOM trees.
    tag: The tag of the elements to find.
  Returns:
    A list of DOM nodes with the specified tag.
  """
  iterator = extract_histograms.IterElementsWithTag
  return list(e for t in trees for e in iterator(t, tag, 2))


def MakeNodeWithChildren(doc, tag, children):
  """Creates a DOM node with specified tag and child nodes.

  Args:
    doc: The document to create the node in.
    tag: The tag to create the node with.
    children: A list of DOM nodes to add as children.
  Returns:
    A DOM node.
  """
  node = doc.createElement(tag)
  for child in children:
    if child.tagName == 'histograms':
      expand_owners.ExpandHistogramsOWNERS(child)
    node.appendChild(child)
  return node


def MergeTrees(trees):
  """Merges a list of histograms.xml DOM trees.

  Args:
    trees: A list of histograms.xml DOM trees.
  Returns:
    A merged DOM tree.
  """
  doc = xml.dom.minidom.Document()
  doc.appendChild(MakeNodeWithChildren(doc, 'histogram-configuration',
    # This can result in the merged document having multiple <enums> and
    # similar sections, but scripts ignore these anyway.
    GetElementsByTagName(trees, 'enums') +
    GetElementsByTagName(trees, 'histograms') +
    GetElementsByTagName(trees, 'histogram_suffixes_list')))
  return doc


def MergeFiles(filenames):
  """Merges a list of histograms.xml files.

  Args:
    filenames: A list of histograms.xml filenames.
  Returns:
    A merged DOM tree.
  """
  trees = [xml.dom.minidom.parse(open(f)) for f in filenames]
  return MergeTrees(trees)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('inputs', nargs="+")
  parser.add_argument('--output', required=True)
  args = parser.parse_args()
  MergeFiles(args.inputs).writexml(open(args.output, 'w'))


if __name__ == '__main__':
  main()
