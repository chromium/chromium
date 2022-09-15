#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that the histograms and variants at given xml have correct prefix."""

import logging
import os
import sys
import xml.dom.minidom

import extract_histograms
import split_xml


def ValidatePrefixInFile(xml_path):
  """Validates that all <histogram> and <variants> are put in the correct file.

  Args:
    xml_path: The path to the histograms.xml file.

  Returns:
    A boolean that is True if at least a histogram has incorrect prefix, False
        otherwise.
  """
  prefix = os.path.basename(os.path.dirname(xml_path))
  has_prefix_error = False
  tree = xml.dom.minidom.parse(xml_path)

  for node in extract_histograms.IterElementsWithTag(tree, 'variants', 3):
    correct_dir = split_xml.GetDirForNode(node)
    if correct_dir != prefix:
      variants_name = node.getAttribute('name')
      logging.error(
          'Variants of name %s is not placed in the correct directory, '
          'please remove it from the metadata/%s directory '
          'and place it in the metadata/%s directory.', variants_name, prefix,
          correct_dir)
      has_prefix_error = True

  for node in extract_histograms.IterElementsWithTag(tree, 'histogram', 3):
    correct_dir = split_xml.GetDirForNode(node)
    if correct_dir != prefix:
      histogram_name = node.getAttribute('name')
      logging.error(
          'Histogram of name %s is not placed in the correct directory, '
          'please remove it from the metadata/%s directory '
          'and place it in the metadata/%s directory.', histogram_name, prefix,
          correct_dir)
      has_prefix_error = True

  return has_prefix_error


def main():
  """Checks that the histograms at given path have prefix that is the dir name.

  Args:
    sys.argv[1]: The relative path to xml file.

  Example usage:
    validate_prefix.py metadata/Fingerprint/histograms.xml
  """
  if len(sys.argv) != 2:
    sys.stderr.write('Usage: %s <rel-path-to-xml>' % sys.argv[0])
    sys.exit(1)

  xml_path = os.path.join(os.getcwd(), sys.argv[1])
  prefix_error = ValidatePrefixInFile(xml_path)

  sys.exit(prefix_error)


if __name__ == '__main__':
  main()
