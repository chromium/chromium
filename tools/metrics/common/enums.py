# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import xml.etree.ElementTree as ET


import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml

_METRIC_FILES_WITH_ENUMS = [
  path_util.GetInputFile('tools/metrics/ukm/ukm.xml'),
  path_util.GetInputFile('tools/metrics/private_metrics/dwa.xml'),
]


def _get_enums_referenced_by_metric_nodes(files: list[str]) -> set[str]:
  """Finds enums used by ukm.xml and similar files."""
  enums_used_in_files = set()

  for file_path in files:
    root = ET.parse(file_path).getroot()
    for node in root.findall('.//metric'):
      if 'enum' in node.attrib:
        enums_used_in_files.add(node.attrib['enum'])

  return enums_used_in_files


def get_all_used_enums(
  histograms: dict[str, extract_histograms.HistogramDict],
) -> set[str]:
  """Finds referenced enum names from parsed histograms and metric files.

  Note that metric files (ukm.xml, dwa.xml) are read from disk.
  """
  enums_used = set()
  for data in histograms.values():
    if 'enumDetails' in data:
      enums_used.add(data['enumDetails']['name'])
  logging.info(f'Found {len(enums_used)} enums from histograms.')

  metric_enum_names = _get_enums_referenced_by_metric_nodes(
    _METRIC_FILES_WITH_ENUMS
  )
  logging.info(
    f'Found {len(metric_enum_names)} enums from ukm and dwa XML files.'
  )

  enums_used.update(metric_enum_names)
  logging.info(f'Found {len(enums_used)} enums total.')
  return enums_used


def get_enums_used_in_files() -> set[str]:
  """Finds referenced enum names from all XML files."""
  logging.info('Reading histogram XML files...')
  merged = merge_xml.MergeFiles(histogram_paths.ALL_XMLS)
  histograms, _ = extract_histograms.ExtractHistogramsFromXmlET(merged)
  return get_all_used_enums(histograms)
