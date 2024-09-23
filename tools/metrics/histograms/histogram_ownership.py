#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple tool to go through histograms.xml and print out the owners for
histograms.
"""

from __future__ import print_function

import os
import sys
from xml.etree import ElementTree as ET

import histogram_paths
import merge_xml

sys.path.append(os.path.join(os.path.dirname(__file__), 'common'))
import path_util


def PrintOwners(root):
  assert root.tag == 'histogram-configuration'
  root_children = root.getchildren()
  histograms = None
  for node in root_children:
    if node.tag == 'histograms':
      histograms = node
      break
  assert histograms != None

  for histogram in histograms.getchildren():
    if histogram.tag != 'histogram':
      continue

    name = histogram.attrib['name']
    owners = []
    for node in histogram.getchildren():
      if node.tag != 'owner':
        continue
      owners.append(node.text)

    if owners:
      print(name, ' '.join(owners))
    else:
      print(name, 'NO_OWNER')


def main():
  """Prints the owners of histograms in a specific file or of all histograms.

  Args:
    argv[1]: Optional argument that is the relative path to a specific
        histograms.xml.

  Example usage to print owners of metadata/Accessibility/histograms.xml:
    python histogram_ownership.py metadata/Accessibility/histograms.xml

  Example usage to print owners of all histograms:
    python histogram_ownership.py
  """
  if len(sys.argv) == 1:
    merged_xml_string = merge_xml.MergeFiles(
        histogram_paths.ALL_XMLS,
        expand_owners_and_extract_components=True).toxml()
    root = ET.fromstring(merged_xml_string)
  else:
    rel_path = path_util.GetInputFile(
        os.path.join('tools', 'metrics', 'histograms', sys.argv[1]))
    if not os.path.exists(rel_path):
      raise ValueError("A histograms.xml file does not exist in %s" % rel_path)

    tree = ET.parse(rel_path)
    root = tree.getroot()

  PrintOwners(root)


if __name__ == '__main__':
  main()
