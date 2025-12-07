#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the histograms XML file is well-formatted."""

import argparse
import logging
import os
import re
import sys
from typing import List
import xml.dom.minidom

import extract_histograms
import histogram_paths
import merge_xml

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import xml_utils

# The allowlist of namespaces (histogram prefixes, case insensitive) that are
# split across multiple files.
_NAMESPACES_IN_MULTIPLE_FILES = [
    'ash', 'autocomplete', 'chromeos', 'fcminvalidations', 'graphics', 'launch',
    'net', 'networkservice', 'usereducation'
]


def CheckNamespaces(xml_paths: List[str]):
  """Check that histograms from a single namespace are all in the same file.

  Generally we want the histograms from a single namespace to be in the same
  file. There are some exceptions to that which are listed in the
  _NAMESPACES_IN_MULTIPLE_FILES variable.

  The namespace is the first component of the name of the histogram. e.g.
  `Foo.Bar.Baz` has a namespace of `Foo`.

  Args:
    xml_paths: A list of paths to the xml files to validate.
  """
  namespaces = {}
  has_errors = False
  for path in xml_paths:
    tree = xml.dom.minidom.parse(path)

    def _GetNamespace(node):
      return node.getAttribute('name').lower().split('.')[0]

    namespaces_in_file = set(
        _GetNamespace(node)
        for node in xml_utils.IterElementsWithTag(tree, 'histogram', depth=3))
    for namespace in namespaces_in_file:
      if (namespace in namespaces
          and namespace not in _NAMESPACES_IN_MULTIPLE_FILES):
        logging.error(
            'Namespace %s has already been used in %s. it\'s recommended to '
            'put histograms with the same namespace in the same file. If you '
            'intentionally want to split a namespace across multiple files, '
            'please add the namespace to the |_NAMESPACES_IN_MULTIPLE_FILES| '
            'in the validate_format.py.' % (namespace, namespaces[namespace]))
        has_errors = True
      namespaces[namespace] = path

  return has_errors


def _CheckVariantsRegistered(xml_paths: List[str]) -> bool:
  """Checks that all tokens within histograms are registered.

  All tokens within histograms should be registered as tokens in the same file
  either inline (as a <token> node) or out of line (as a <variants> node).

  Args:
    xml_paths: A list of paths to the xml files to validate.
  """
  has_errors = False
  for path in xml_paths:
    tree = xml.dom.minidom.parse(path)
    variants, variants_errors = extract_histograms.ExtractVariantsFromXmlTree(
        tree)
    has_errors = has_errors or bool(variants_errors)

    for histogram in xml_utils.IterElementsWithTag(tree, 'histogram', depth=3):
      tokens, tokens_errors = extract_histograms.ExtractTokens(
          histogram, variants)
      has_errors = has_errors or bool(tokens_errors)

      token_keys = [token['key'] for token in tokens]
      token_keys.extend(variants.keys())

      histogram_name = histogram.getAttribute('name')

      tokens_in_name = re.findall(r'\{(.+?)\}', histogram_name)
      for used_token in tokens_in_name:
        if used_token not in token_keys:
          logging.error(
              'Token {%s} is not registered in histogram %s in file %s.',
              used_token, histogram_name, path)
          has_errors = True

  return has_errors


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--xml_paths',
      type=str,
      nargs='*',
      default=histogram_paths.ALL_XMLS,
      help='An optional list of paths to XML files to validate passed as'
      ' consecutive arguments. Production XML files are validated by default.')
  paths_to_check = parser.parse_args().xml_paths

  doc = merge_xml.MergeFiles(paths_to_check,
                             expand_owners_and_extract_components=False)
  _, errors = extract_histograms.ExtractHistogramsFromDom(doc)
  errors = errors or CheckNamespaces(paths_to_check)
  errors = errors or _CheckVariantsRegistered(paths_to_check)
  sys.exit(bool(errors))


if __name__ == '__main__':
  main()
