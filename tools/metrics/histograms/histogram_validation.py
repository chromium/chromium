# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pure validation logic for histograms.

This module contains standalone, testable validation functions for Chromium
histograms. These checks are typically called by presubmit scripts (e.g.
PRESUBMIT.py) to enforce styling, correctness, and safety constraints
before changes are committed.
"""

import dataclasses
import pathlib
import re
from typing import Callable, List, Set, Tuple
import xml.dom.minidom

import chromium_src.tools.metrics.histograms.histogram_utils as histogram_utils
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml


@dataclasses.dataclass
class HistogramFileState:
  path: str
  old_contents: List[str]
  new_contents: List[str]
  action: str


@dataclasses.dataclass
class AffectedFileForLineCheck:
  path: str
  changed_lines: List[Tuple[int, str]]  # list of (line_number, line_content)


@dataclasses.dataclass
class GetOldAndNewVariantsResult:
  variants_file_path: str
  old_variants: str
  new_variants: str
  is_modified: bool


@dataclasses.dataclass
class GetFilesToCheckResult:
  files_to_check: List[HistogramFileState]
  old_variants_doc: xml.dom.minidom.Document | None
  new_variants_doc: xml.dom.minidom.Document | None
  modified_variants_blocks: Set[str]


def get_old_and_new_variants(
  affected_files: List[HistogramFileState],
  read_file_fn: Callable[[str], str],
  variants_paths: List[str],
) -> List[GetOldAndNewVariantsResult]:
  """Retrieves the old and new contents of variants XML files.

  Args:
    affected_files: List of files affected by changes in the patchset.
    read_file_fn: Function to read file content for unmodified files.
    variants_paths: List of variant XML file paths to retrieve contents for.

  Returns:
    A list of GetOldAndNewVariantsResult, detailing the old and new XML
    contents for each variants file, and whether the file was modified in
    the current patchset.
  """
  affected_files_by_path = {
    pathlib.Path(f.path).resolve(): f for f in affected_files
  }
  result = []
  for variant_file in variants_paths:
    resolved_variant_file = pathlib.Path(variant_file).resolve()
    affected_file = affected_files_by_path.get(resolved_variant_file)
    if affected_file:
      old_variants = '\n'.join(affected_file.old_contents)
      new_variants = '\n'.join(affected_file.new_contents)
      is_modified = True
    else:
      content = read_file_fn(str(resolved_variant_file))
      old_variants = content
      new_variants = content
      is_modified = False

    result.append(
      GetOldAndNewVariantsResult(
        variants_file_path=str(resolved_variant_file),
        old_variants=old_variants,
        new_variants=new_variants,
        is_modified=is_modified,
      )
    )
  return result


def _get_histograms_affected_by_variant_changes(
  modified_variants_blocks: Set[str],
  histograms_paths: List[str],
) -> List[str]:
  return histogram_utils.find_files_using_variants(
    modified_variants_blocks, histograms_paths
  )


def get_virtually_affected_files_to_check(
  virtually_affected_paths: List[str],
  processed_paths: Set[pathlib.Path],
  read_file_fn: Callable[[str], str],
) -> List[HistogramFileState]:
  """Reads virtually affected files and returns them as HistogramFileState.

  Virtually affected files are those which are not directly modified in the
  patchset, but reference a variant that was modified or added. We need to
  validate them because the change in variant definition might make the
  histogram names invalid or trigger limits.

  Args:
    virtually_affected_paths: List of paths to virtually affected files.
    processed_paths: Set of paths that have already been processed as
      directly modified (to avoid duplicate checks).
    read_file_fn: Function to read file content.

  Returns:
    A list of HistogramFileState for the virtually affected files.
  """
  files_to_check: List[HistogramFileState] = []
  for path in virtually_affected_paths:
    resolved_path = pathlib.Path(path).resolve()
    if resolved_path in processed_paths:
      continue
    content = read_file_fn(str(resolved_path)).splitlines()
    files_to_check.append(
      HistogramFileState(
        path=str(resolved_path),
        old_contents=content,
        new_contents=content,
        action='M',
      )
    )
  return files_to_check


def get_files_to_check(
  affected_files: List[HistogramFileState],
  read_file_fn: Callable[[str], str],
  variants_paths: List[str],
  histograms_paths: List[str],
) -> GetFilesToCheckResult:
  """Returns a list of files to check for histogram changes."""
  variants_res = get_old_and_new_variants(
    affected_files, read_file_fn, variants_paths
  )

  old_variants_trees = []
  new_variants_trees = []
  virtually_affected_paths = []
  modified_variants_blocks = set()

  for res in variants_res:
    old_content = res.old_variants.strip()
    new_content = res.new_variants.strip()

    old_doc = xml.dom.minidom.parseString(old_content) if old_content else None
    new_doc = xml.dom.minidom.parseString(new_content) if new_content else None
    if old_doc:
      old_variants_trees.append(old_doc)
    if new_doc:
      new_variants_trees.append(new_doc)

    if res.is_modified:
      file_modified_variants = histogram_utils.get_modified_variants_blocks(
        res.old_variants, res.new_variants
      )
      modified_variants_blocks.update(file_modified_variants)
      virtually_affected_paths.extend(
        _get_histograms_affected_by_variant_changes(
          file_modified_variants,
          histograms_paths,
        )
      )

  old_variants_doc = (
    merge_xml.MergeTreesDeprecated(
      old_variants_trees, should_expand_owners=False
    )
    if old_variants_trees
    else None
  )
  new_variants_doc = (
    merge_xml.MergeTreesDeprecated(
      new_variants_trees, should_expand_owners=False
    )
    if new_variants_trees
    else None
  )

  resolved_histograms_paths = {
    pathlib.Path(p).resolve() for p in histograms_paths
  }
  files_to_check = []
  processed_paths = set()
  for f in affected_files:
    resolved_path = pathlib.Path(f.path).resolve()
    if resolved_path in resolved_histograms_paths:
      if resolved_path not in processed_paths:
        files_to_check.append(f)
        processed_paths.add(resolved_path)

  virt_files = get_virtually_affected_files_to_check(
    virtually_affected_paths, processed_paths, read_file_fn
  )
  files_to_check.extend(virt_files)

  return GetFilesToCheckResult(
    files_to_check,
    old_variants_doc,
    new_variants_doc,
    modified_variants_blocks,
  )


def _empty_variants_doc() -> xml.dom.minidom.Document:
  return xml.dom.minidom.parseString(
    '<variants-configuration></variants-configuration>'
  )


def get_histogram_names(
  files_contents: List[List[str]],
  variants: xml.dom.minidom.Document | None,
) -> Set[str]:
  """Returns all expanded histogram names from the given files contents."""
  all_histograms = set()
  variants_doc = variants or _empty_variants_doc()
  for contents in files_contents:
    all_histograms.update(
      histogram_utils.get_names_from_contents(contents, variants_doc)
    )
  return all_histograms


def get_histograms_with_modified_variants(
  files_to_check: List[HistogramFileState],
  old_variants: xml.dom.minidom.Document | None,
  new_variants: xml.dom.minidom.Document | None,
  modified_variants_blocks: Set[str],
) -> Set[str]:
  """Returns existing histograms that reference modified `<variants>` blocks."""
  old_variants_doc = old_variants or _empty_variants_doc()
  new_variants_doc = new_variants or _empty_variants_doc()
  variant_modified_histograms = set()

  for file_state in files_to_check:
    file_modified_variants = histogram_utils.get_modified_variants_blocks(
      '\n'.join(file_state.old_contents), '\n'.join(file_state.new_contents)
    )
    all_modified_variants = modified_variants_blocks | file_modified_variants
    if not all_modified_variants:
      continue

    old_histograms = set()
    if file_state.action != 'A':
      old_histograms = histogram_utils.get_names_from_contents(
        file_state.old_contents, old_variants_doc
      )
      variant_modified_histograms.update(
        histogram_utils.get_names_using_variants_from_contents(
          file_state.old_contents, old_variants_doc, all_modified_variants
        )
      )

    if file_state.action != 'D':
      new_variant_modified_histograms = (
        histogram_utils.get_names_using_variants_from_contents(
          file_state.new_contents, new_variants_doc, all_modified_variants
        )
      )
      variant_modified_histograms.update(
        new_variant_modified_histograms & old_histograms
      )

  return variant_modified_histograms


def check_booleans_are_enums(
  affected_files: List[AffectedFileForLineCheck],
) -> List[Tuple[str, int, str]]:
  """Checks that histograms that use Booleans do not use units."""
  results = []
  inclusion_pattern = re.compile(r'units="[Bb]oolean')

  for affected_file in affected_files:
    if 'histograms.xml' in affected_file.path:
      for line_number, line in affected_file.changed_lines:
        if inclusion_pattern.search(line):
          results.append((affected_file.path, line_number, line))
  return results


def check_removed_segmentation_histograms(
  removed_histograms: Set[str],
  segmentation_histograms: Set[str],
) -> Set[str]:
  """Checks if any histogram used by segmentation platform is removed."""
  return removed_histograms.intersection(segmentation_histograms)


def check_if_introduced_too_many_histograms(
  added_histograms: Set[str],
  threshold: int,
) -> bool:
  """Checks if the number of histograms introduced in the CL is too high."""
  return len(added_histograms) > threshold
