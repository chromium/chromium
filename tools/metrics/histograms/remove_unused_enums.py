#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Removes unused enums from enums.xml files."""

import io
import logging
import os
import sys
from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))

import extract_histograms
import histogram_configuration_model
import histogram_paths
import merge_xml
import path_util


def _get_enums_from_histogram_files(files: list[str]):
  """Finds the names of all referenced enums from the specified XML files."""
  merged = merge_xml.MergeFiles(files)
  histograms, _ = extract_histograms.ExtractHistogramsFromDom(merged)
  enums_used_in_file = set()
  for histogram_name, data in histograms.items():
    # Skip non-enum histograms.
    if 'enumDetails' not in data:
      continue
    enum_name = data['enumDetails']['name']
    enums_used_in_file.add(enum_name)
  return enums_used_in_file


def _get_enums_referenced_by_metric_nodes_in_xml_files(files: list[str]):
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


def _remove_enum_nodes_not_in_list(enum_names, filepath):
  """Returns the <enum> nodes not corresponding to the specified names."""
  with io.open(filepath, 'r', encoding='utf-8') as f:
    document = minidom.parse(f)

  enum_nodes = []
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value not in enum_names:
      enum_nodes.append(enum_node)

  for node in enum_nodes:
    node.parentNode.removeChild(node)

  xml_with_nodes_removed = histogram_configuration_model.PrettifyTree(document)
  return enum_nodes, xml_with_nodes_removed


def _remove_unused_enums():
  """Removes unused enums from ALL enums.xml files."""
  print(f'Reading histogram XML files...')
  enum_names = _get_enums_from_histogram_files(histogram_paths.ALL_XMLS)
  print(f'Found {len(enum_names)} enums from histograms.')

  metric_files = [
      histogram_paths.UKM_XML,
      histogram_paths.DWA_XML,
  ]
  metric_enum_names = (
      _get_enums_referenced_by_metric_nodes_in_xml_files(metric_files))
  print(f'Found {len(metric_enum_names)} enums from ukm and dwa.')

  enum_names.update(metric_enum_names)
  print(f'Found {len(enum_names)} enums total.')

  for enum_file in histogram_paths.ENUMS_XMLS:
    enum_nodes, updated_enum_xml = _remove_enum_nodes_not_in_list(
        enum_names, enum_file)
    if not enum_nodes:
      print(f'All enums in {enum_file} are referenced')
      continue

    print(f'Removing {len(enum_nodes)} enum nodes from {enum_file}:')
    for enum_node in enum_nodes:
      enum_name = enum_node.attributes['name'].value
      print(f' - {enum_name}')

    with open(enum_file, 'w', encoding='utf-8', newline='') as f:
      f.write(updated_enum_xml)
    print('File updated.')


if __name__ == '__main__':
  _remove_unused_enums()
