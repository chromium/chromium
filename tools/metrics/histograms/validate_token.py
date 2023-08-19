#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that the histograms at given xml use variants defined in the file."""

import logging
import os
import sys
import xml.dom.minidom

import xml_utils


def ValidateTokenInFile(xml_path: str) -> bool:
  """Validates that all <token> uses <variants> defined in the file.

  Args:
    xml_path: The path to the histograms.xml file.

  Returns:
    A boolean that is True if at least a histogram uses a <variants> not
        defined in the file, False otherwise.
  """
  has_token_error: bool = False
  tree: xml.dom.minidom.Document = xml.dom.minidom.parse(xml_path)
  variants: list[str] = []

  for node in xml_utils.IterElementsWithTag(tree, 'variants', 3):
    variants_name: str = node.getAttribute('name')
    variants.append(variants_name)

  for histogram in xml_utils.IterElementsWithTag(tree, 'histogram', 3):
    erroneous_tokens: list[str] = []
    for node in xml_utils.IterElementsWithTag(histogram, 'token', 1):
      if node.hasAttribute('variants'):
        if node.getAttribute('variants') not in variants:
          erroneous_tokens.append(node.getAttribute('key'))
    if erroneous_tokens:
      histogram_name: str = histogram.getAttribute('name')
      logging.error(
          'Token(s) %s in histogram %s are using variants not defined in the '
          'file, please define them before use.', ', '.join(erroneous_tokens),
          histogram_name)
      has_token_error = True

  return has_token_error


def main():
  """Checks that the histograms at given path use variants defined in the file.

  Args:
    sys.argv[1]: The relative path to xml file.

  Example usage:
    validate_token.py metadata/Fingerprint/histograms.xml
  """
  if len(sys.argv) != 2:
    sys.stderr.write('Usage: %s <rel-path-to-xml>' % sys.argv[0])
    sys.exit(1)

  xml_path: str = os.path.join(os.getcwd(), sys.argv[1])
  token_error: bool = ValidateTokenInFile(xml_path)

  sys.exit(token_error)


if __name__ == '__main__':
  main()
