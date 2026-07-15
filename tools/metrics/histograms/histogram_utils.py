# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions for parsing and processing histogram XML files."""

import os
import pathlib
import re
from typing import Callable, Iterable, List, Set
import xml.dom.minidom
import xml.parsers.expat

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.xml_utils as xml_utils
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml


def get_names(xml_files):
  """Returns all histogram names generated from a list of xml files.

  Args:
    xml_files: A list of open file objects containing histogram definitions.
  Returns:
    The set of histogram names.
  """
  doc = merge_xml.MergeFiles(files=xml_files)
  histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(doc)
  if had_errors:
    raise ValueError('Error parsing inputs.')
  return set(extract_histograms.ExtractNames(histograms))


def _parse_default_variants() -> xml.dom.minidom.Document:
  variants_path = os.path.join(os.path.dirname(__file__), 'variants.xml')
  return xml.dom.minidom.parse(variants_path)


def get_names_from_contents(contents: Iterable[str],
                            variants_doc: xml.dom.minidom.Document) -> Set[str]:
  """Returns all histogram names from the given contents.

  This function is different from get_names() in that it does not make
  additional checks against the given contents.

  Args:
    contents: An iterable of strings from the raw histograms xml file.
    variants_doc: Pre-parsed variants.xml DOM Document to use for
      expansion.

  Returns:
    The set of histogram names.
  """
  joined_contents = '\n'.join(contents)
  if not joined_contents.strip():
    return set()

  content_doc = xml.dom.minidom.parseString(joined_contents)
  doc = _merge_histograms_with_variants(content_doc, variants_doc)

  histograms, _ = extract_histograms.ExtractHistogramsFromDom(doc)
  return set(extract_histograms.ExtractNames(histograms))


def _merge_histograms_with_variants(
    content_doc: xml.dom.minidom.Document,
    variants_doc: xml.dom.minidom.Document) -> xml.dom.minidom.Document:
  variants_clone = variants_doc.cloneNode(True)
  return merge_xml.MergeTrees([content_doc, variants_clone],
                              should_expand_owners=False)


def get_modified_variants_blocks(old_content: str,
                                 new_content: str) -> Set[str]:
  """Returns the names of <variants> blocks modified between old and new
  content.
  """

  def _get_variants(content):
    if not content.strip():
      return {}
    doc = xml.dom.minidom.parseString(content)
    xml_utils.NormalizeAllAttributeValues(doc)
    variants, _ = extract_histograms.ExtractVariantsFromXmlTree(doc)
    return variants

  old_vars = _get_variants(old_content)
  new_vars = _get_variants(new_content)

  modified_token_names = set()

  for token_name, var_list in new_vars.items():
    if token_name not in old_vars:
      modified_token_names.add(token_name)
      continue

    old_var_names = {v['name'] for v in old_vars[token_name]}
    new_var_names = {v['name'] for v in var_list}
    # TODO(crbug.com/525692876): Also check if other attributes of variants
    # (e.g. summary, obsolete, owners) changed.
    if old_var_names != new_var_names:
      modified_token_names.add(token_name)

  for token_name in old_vars.keys():
    if token_name not in new_vars:
      modified_token_names.add(token_name)

  return modified_token_names


# A regular expression that matches two patterns in histogram XML files:
# 1. Explicit variants attribute: e.g. variants='VariantName'
#    Matches `variants='...'` and captures the variant block
#    name in Group 1.
# 2. Token placeholders in histogram names: e.g. {TokenName}
#    Matches `{...}` and captures the token key name in Group 2.
#
# Note: Group 2 captures all token placeholders in curly braces, not just those
# using variants. This broad match supports implicit token variants (where the
# token key matches the variant block name).
VARIANTS_PATTERN = re.compile(r'variants\s*=\s*["\']([^"\']+)["\']|\{([^}]+)\}')


def _path_contents(path: str) -> str:
  with open(path, 'r', encoding='utf-8') as f:
    return f.read()


def _has_any_variants(content: str, variant_names: Set[str]) -> bool:
  # Performance optimization: use regex search on raw file content to quickly
  # scan if a file uses any of the modified variant blocks. This avoids the
  # significant overhead of parsing multiple large XML files with minidom.
  for match in VARIANTS_PATTERN.finditer(content):
    val = match.group(1) or match.group(2)
    if val in variant_names:
      return True
  return False


def find_files_using_variants(
    variant_names: Set[str],
    histograms_paths: List[str] = histogram_paths.HISTOGRAMS_XMLS) -> List[str]:
  """Returns paths to histograms.xml files using any of the variant_names."""
  if not variant_names:
    return []

  matching_files = []
  for path in histograms_paths:
    content = _path_contents(path)
    if _has_any_variants(content, variant_names):
      matching_files.append(path)
  return matching_files
