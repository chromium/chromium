#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints all histogram names."""

from __future__ import print_function

import argparse
import io
import os
import re
import subprocess
import sys
from typing import Any, Callable, Iterable, List, Set
import xml.dom.minidom
import xml.parsers.expat

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.xml_utils as xml_utils
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml

# Used in android_webview/java/res/raw/histograms_allowlist_check.py.
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
    raise ValueError("Error parsing inputs.")
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
  # contents is an iterator, so convert to list to be able to reuse it.
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


def histogram_xml_files():
  return [open(f, encoding="utf-8") for f in histogram_paths.ALL_XMLS]


def get_histogram_diff(revision):
  """Returns the added / removed histogram names relative to git revision

  Args:
    revision: A git revision as described in
      https://git-scm.com/docs/gitrevisions
  Returns:
    A tuple of (added names, removed names), where each entry is sorted in
    ascending order.
  """

  def get_file_at_revision(path):
    """Returns a file-like object containing |path|'s content at |revision|"""
    obj = "%s:%s" % (revision, path)
    contents = subprocess.check_output(
        ("git", "cat-file", "--textconv", obj)).decode()

    # Just store the contents in memory. histograms.xml is big, but it isn't
    # _that_ big.
    return io.StringIO(contents)

  prev_files = []
  for p in histogram_paths.ALL_XMLS_RELATIVE:
    try:
      prev_files.append(get_file_at_revision(p))
    except subprocess.CalledProcessError:
      # Paths might not exist in the provided revision.
      continue

  current_histogram_names = get_names(histogram_xml_files())
  prev_histogram_names = get_names(prev_files)

  added_names = sorted(list(current_histogram_names - prev_histogram_names))
  removed_names = sorted(list(prev_histogram_names - current_histogram_names))
  return (added_names, removed_names)


def _print_diff_names(revision):
  added_names, removed_names = get_histogram_diff(revision)
  print("%d histograms added:" % len(added_names))
  for name in added_names:
    print(name)

  print("%d histograms removed:" % len(removed_names))
  for name in removed_names:
    print(name)


def main(argv):
  parser = argparse.ArgumentParser(description="Print histogram names.")
  parser.add_argument("--diff",
                      type=str,
                      help="Git revision to diff against (e.g. HEAD~)")
  args = parser.parse_args(argv[1:])
  if args.diff is not None:
    _print_diff_names(args.diff)
  else:
    name_set = get_names(histogram_xml_files())
    for name in sorted(list(name_set)):
      print(name)


if __name__ == "__main__":
  main(sys.argv)
