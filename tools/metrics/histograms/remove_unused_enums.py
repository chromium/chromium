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

import enums
import histogram_configuration_model
import histogram_paths


def _remove_enum_nodes_not_in_list(
    enum_names: set[str], filepath: str) -> tuple[list[minidom.Element], str]:
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
  enum_names = enums.get_enums_used_in_files()

  for enum_file in histogram_paths.ENUMS_XMLS:
    enum_nodes, updated_enum_xml = _remove_enum_nodes_not_in_list(
        enum_names, enum_file)
    if not enum_nodes:
      logging.info(f'All enums in {enum_file} are referenced')
      continue

    logging.info(f'Removing {len(enum_nodes)} enum nodes from {enum_file}:')
    for enum_node in enum_nodes:
      enum_name = enum_node.attributes['name'].value
      print(f' - {enum_name}')

    with open(enum_file, 'w', encoding='utf-8', newline='') as f:
      f.write(updated_enum_xml)
    logging.info('File updated.')


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO,
                      stream=sys.stderr,
                      format='%(message)s')
  _remove_unused_enums()
