# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions for parsing and processing histogram XML files."""

import copy
import os
import pathlib
import re
from typing import Callable, Iterable, List, Set
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.xml_utils as xml_utils
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml


TOKEN_PLACEHOLDER_PATTERN = re.compile(r'\{([^}]+)\}')


def get_names(xml_files):
  """Returns all histogram names generated from a list of xml files.

  Args:
    xml_files: A list of open file objects containing histogram definitions.
  Returns:
    The set of histogram names.
  """
  root = merge_xml.MergeFiles(files=xml_files)
  histograms, had_errors = extract_histograms.ExtractHistogramsFromXmlET(root)
  if had_errors:
    raise ValueError('Error parsing inputs.')
  return set(extract_histograms.ExtractNames(histograms))


def _parse_default_variants() -> ET.Element:
  variants_path = os.path.join(os.path.dirname(__file__), 'variants.xml')
  return ET.parse(variants_path).getroot()


def get_names_from_contents(
  contents: Iterable[str], variants_doc: ET.Element
) -> Set[str]:
  """Returns all histogram names from the given contents.

  This function is different from get_names() in that it does not make
  additional checks against the given contents.

  Args:
    contents: An iterable of strings from the raw histograms xml file.
    variants_doc: Pre-parsed variants.xml ElementTree root to use for
      expansion.

  Returns:
    The set of histogram names.
  """
  joined_contents = '\n'.join(contents)
  if not joined_contents.strip():
    return set()

  root = ET.fromstring(joined_contents)
  doc = _merge_histograms_with_variants(root, variants_doc)

  histograms, _ = extract_histograms.ExtractHistogramsFromXmlET(doc)
  return set(extract_histograms.ExtractNames(histograms))


def _merge_histograms_with_variants(
  content_doc: ET.Element, variants_doc: ET.Element
) -> ET.Element:
  variants_clone = copy.deepcopy(variants_doc)
  return merge_xml.MergeTrees(
    [content_doc, variants_clone], should_expand_owners=False
  )


def _get_canonical_variants(variants: List[extract_histograms.VariantDict]):
  canonical_variants = []
  for variant in variants:
    canonical_variant = []
    for key, value in sorted(variant.items()):
      if isinstance(value, list):
        value = tuple(sorted(value))
      canonical_variant.append((key, value))
    canonical_variants.append(tuple(canonical_variant))
  return tuple(sorted(canonical_variants))


def get_modified_variants_blocks(
  old_content: str, new_content: str
) -> Set[str]:
  """Returns the names of <variants> blocks modified between old and new
  content.
  """

  def _get_variants(content):
    if not content.strip():
      return {}
    doc = ET.fromstring(content)
    xml_utils.NormalizeAllAttributeValues(doc)
    variants, _ = extract_histograms.ExtractVariantsFromXmlTree(doc)
    return variants

  old_vars = _get_variants(old_content)
  new_vars = _get_variants(new_content)

  modified_token_names = set()
  all_token_names = set(old_vars.keys()) | set(new_vars.keys())
  for token_name in all_token_names:
    if _get_canonical_variants(old_vars.get(token_name, [])) != (
      _get_canonical_variants(new_vars.get(token_name, []))
    ):
      modified_token_names.add(token_name)

  return modified_token_names


def _histogram_references_global_variant_from_list(
  histogram: ET.Element, variant_names: Set[str]
) -> bool:
  """Returns whether |histogram| references a global variant in |variant_names|.

  For each placeholder in the histogram name, this checks whether the
  placeholder name is in |variant_names| and is not overridden by a local
  `<token>`. It also checks for local `<token>` elements that explicitly
  reference a variant in |variant_names| through their `variants` attribute.

  Args:
    histogram: The `<histogram>` element to inspect.
    variant_names: Names of global `<variants>` blocks to match.
  """
  tokens = list(xml_utils.IterElementsWithTag(histogram, 'token', 1))
  token_keys = {token.get('key') for token in tokens if token.get('key')}
  hist_name = histogram.get('name') or ''
  for match in TOKEN_PLACEHOLDER_PATTERN.finditer(hist_name):
    token_name = match.group(1)
    # The placeholder names a global variant from the provided list and is not
    # overridden by a local token.
    if token_name not in token_keys and token_name in variant_names:
      return True

  for token in tokens:
    # The local token explicitly references a global variant from the list.
    variant_attr = token.get('variants')
    if variant_attr and variant_attr in variant_names:
      return True

  return False


def _remove_affected_histogram_references_from_tree(
  root: ET.Element, histogram_names: Set[str]
) -> None:
  """Removes matching `<affected-histogram>` elements from |root|.

  This modifies |root| in place after
  `get_names_using_variants_from_contents()` filters its `<histogram>`
  elements. Removing references to filtered histograms prevents suffix
  expansion from logging an error about a missing histogram.
  """
  if not histogram_names:
    return

  for suffixes in xml_utils.IterElementsWithTag(root, 'histogram_suffixes', 2):
    for affected in list(
      xml_utils.IterElementsWithTag(suffixes, 'affected-histogram', 1)
    ):
      if affected.get('name') in histogram_names:
        suffixes.remove(affected)


def get_names_using_variants_from_contents(
  contents: Iterable[str],
  variants_doc: ET.Element,
  variant_names: Set[str],
) -> Set[str]:
  """Returns expanded names from one histogram XML file using |variant_names|.

  |contents| is the raw line sequence for one histogram XML file. The function
  parses it into a temporary ElementTree, removes histograms that do not
  reference a changed global variants block, then merges the remaining tree with
  |variants_doc| and expands their names.

  Args:
    contents: Raw lines from one histogram XML file.
    variants_doc: Merged global variants ElementTree root used for name
      expansion.
    variant_names: Names of changed global `<variants>` blocks.
  """
  joined_contents = '\n'.join(contents)
  if not joined_contents.strip() or not variant_names:
    return set()

  content_doc = ET.fromstring(joined_contents)
  histogram_names = set()
  for histograms in xml_utils.IterElementsWithTag(content_doc, 'histograms', 2):
    for histogram in list(
      xml_utils.IterElementsWithTag(histograms, 'histogram', 1)
    ):
      if not _histogram_references_global_variant_from_list(
        histogram, variant_names
      ):
        name = histogram.get('name')
        if name:
          histogram_names.add(name)
        histograms.remove(histogram)

  _remove_affected_histogram_references_from_tree(content_doc, histogram_names)

  merged_et = _merge_histograms_with_variants(content_doc, variants_doc)
  histograms_dict, _ = extract_histograms.ExtractHistogramsFromXmlET(merged_et)
  return set(extract_histograms.ExtractNames(histograms_dict))


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
  histograms_paths: List[str] = histogram_paths.HISTOGRAMS_XMLS,
) -> List[str]:
  """Returns paths to histograms.xml files using any of the variant_names."""
  if not variant_names:
    return []

  matching_files = []
  for path in histograms_paths:
    content = _path_contents(path)
    if _has_any_variants(content, variant_names):
      matching_files.append(path)
  return matching_files
