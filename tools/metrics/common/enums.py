# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
from xml.dom import minidom

import path_util

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'histograms'))

import extract_histograms
import histogram_paths
import merge_xml

_METRIC_FILES_WITH_ENUMS = [
    path_util.GetInputFile('tools/metrics/ukm/ukm.xml'),
    path_util.GetInputFile('tools/metrics/private_metrics/dkm.xml'),
    path_util.GetInputFile('tools/metrics/private_metrics/dwa.xml'),
]


def _get_enums_from_histogram_files(files: list[str]) -> set[str]:
  """Finds the names of all referenced enums from the specified XML files."""
  merged = merge_xml.MergeFiles(files)
  histograms, _ = extract_histograms.ExtractHistogramsFromDom(merged)
  enums_used_in_file = set()
  for _, data in histograms.items():
    # Skip non-enum histograms.
    if 'enumDetails' not in data:
      continue
    enum_name = data['enumDetails']['name']
    enums_used_in_file.add(enum_name)
  return enums_used_in_file


def _get_enums_referenced_by_metric_nodes(files: list[str]) -> set[str]:
  """Finds enums used by ukm.xml and similar files."""
  enums_used_in_files = set()

  for file_path in files:
    with open(file_path, 'r') as f:
      document = minidom.parse(f)

    for node in document.getElementsByTagName('metric'):
      if not 'enum' in node.attributes:
        continue
      enums_used_in_files.add(node.attributes['enum'].value)

  return enums_used_in_files


def get_enums_used_in_files() -> set[str]:
  """Finds the names of all referenced enums from the specified XML files."""
  logging.info(f'Reading histogram XML files...')
  enum_names = _get_enums_from_histogram_files(histogram_paths.ALL_XMLS)
  logging.info(f'Found {len(enum_names)} enums from histograms.')

  metric_enum_names = (
      _get_enums_referenced_by_metric_nodes(_METRIC_FILES_WITH_ENUMS))
  logging.info(f'Found {len(metric_enum_names)} enums from ukm, dkm, and dwa.')

  enum_names.update(metric_enum_names)
  logging.info(f'Found {len(enum_names)} enums total.')
  return enum_names
