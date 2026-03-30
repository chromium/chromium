#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that histograms XML files are well-formatted."""

import argparse
import io
import logging
import os
import re
import sys
from typing import List
import xml.dom.minidom

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.enums as enums
import chromium_src.tools.metrics.common.xml_utils as xml_utils
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml


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
  namespaces: dict[str, str] = {}
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


def _IsGlobalVariantFile(path: str) -> bool:
  return path.endswith(
      os.path.join('tools', 'metrics', 'histograms', 'variants.xml'))


def _CheckVariantsRegistered(xml_paths: List[str]) -> bool:
  """Checks that all tokens within histograms are registered.

  Tokens within histograms should be registered as tokens either inline
  (as a <token> node) or out of line (as a <variants> node) in the file where
  it is used, or in the global `variants.xml` file.

  Args:
    xml_paths: A list of paths to the xml files to validate.
  """
  has_errors = False

  global_variants = {}
  for path in xml_paths:
    if _IsGlobalVariantFile(path):
      tree = xml.dom.minidom.parse(path)
      variants, variants_errors = extract_histograms.ExtractVariantsFromXmlTree(
          tree)
      has_errors = has_errors or bool(variants_errors)
      global_variants.update(variants)
      break

  for path in xml_paths:
    if _IsGlobalVariantFile(path):
      continue

    tree = xml.dom.minidom.parse(path)
    variants, variants_errors = extract_histograms.ExtractVariantsFromXmlTree(
        tree)
    has_errors = has_errors or bool(variants_errors)

    merged_variants = dict(variants)
    merged_variants.update(global_variants)

    for histogram in xml_utils.IterElementsWithTag(tree, 'histogram', depth=3):
      tokens, tokens_errors = extract_histograms.ExtractTokens(
          histogram, merged_variants)
      has_errors = has_errors or bool(tokens_errors)

      token_keys = [token['key'] for token in tokens]
      token_keys.extend(merged_variants.keys())

      histogram_name = histogram.getAttribute('name')

      tokens_in_name = re.findall(r'\{(.+?)\}', histogram_name)
      for used_token in tokens_in_name:
        if used_token not in token_keys:
          logging.error(
              'Token {%s} is not registered in histogram %s in file %s.',
              used_token, histogram_name, path)
          has_errors = True

  return has_errors


def _CheckNoUnusedEnums(xml_paths: List[str]) -> bool:
  """Checks that all enums are referenced by metrics."""
  enum_names = enums.get_enums_used_in_files()

  has_errors = False
  for enum_file in xml_paths:
    with io.open(enum_file, 'r', encoding='utf-8') as f:
      document = xml.dom.minidom.parse(f)
      for enum_node in document.getElementsByTagName('enum'):
        if enum_node.attributes['name'].value not in enum_names:
          logging.error(
              'Enum %s from file %s/enums.xml is not referenced by any metric.',
              enum_node.attributes['name'].value,
              os.path.basename(os.path.dirname(enum_file)),
          )
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
  errors = errors or _CheckNoUnusedEnums(paths_to_check)
  sys.exit(bool(errors))


if __name__ == '__main__':
  main()
