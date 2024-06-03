#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Removes unused enums from the monolithic enums.xml file.

Note: This does not handle sharded enums.xml files yet.
"""

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


def _get_enums_from_histogram_files(files):
  """Finds the names of all referenced enums from the specified XML files."""
  merged = merge_xml.MergeFiles(files)
  histograms, _ = extract_histograms.ExtractHistogramsFromDom(merged)
  enums_used_in_file = set()
  for histogram_name, data in histograms.items():
    # Skip non-enum histograms.
    if 'enum' not in data:
      continue
    enum_name = data['enum']['name']
    enums_used_in_file.add(enum_name)
  return enums_used_in_file


def _get_enums_from_ukm():
  """Finds enums used by ukm.xml."""
  with open(histogram_paths.UKM_XML, 'r') as f:
    document = minidom.parse(f)

  enums_used_in_file = set()
  for node in document.getElementsByTagName('metric'):
    if not 'enum' in node.attributes:
      continue
    enums_used_in_file.add(node.attributes['enum'].value)

  return enums_used_in_file


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
  print(f'Reading XML files...')
  enum_names = _get_enums_from_histogram_files(histogram_paths.ALL_XMLS)
  print(f'Found {len(enum_names)} enums from histograms.')

  ukm_enum_names = _get_enums_from_ukm()
  print(f'Found {len(ukm_enum_names)} enums from ukm.')

  enum_names.update(ukm_enum_names)
  print(f'Found {len(enum_names)} enums total.')

  for enum_file in histogram_paths.ENUMS_XMLS:
    enum_nodes, updated_enum_xml = _remove_enum_nodes_not_in_list(
        enum_names, enum_file)
    print(f'Removed {len(enum_nodes)} that were not referenced.')

    print(f'Writing updated file: {enum_file}')
    with open(enum_file, 'w', encoding='utf-8', newline='') as f:
      f.write(updated_enum_xml)


if __name__ == '__main__':
  _remove_unused_enums()
