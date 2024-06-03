#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Extracts enums.xml to separate files.

Usage:
  tools/metrics/histograms/split_enums.py <dir_name>

Where <dir_name> is a subdirectory of tools/metrics/histograms/metadata/ where
a new enums.xml file should be populated from enums referenced by the
histograms.xml in that directory.
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

ENUMS_XML_TEMPLATE = """<!--
Copyright 2024 The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
-->

<!--

This file describes the enumerations referenced by entries in histograms.xml for
this directory. Some enums may instead be listed in the central enums.xml file
at src/tools/metrics/histograms/enums.xml when multiple files use them.

For best practices on writing enumerations descriptions, see
https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#Enum-Histograms

Please follow the instructions in the OWNERS file in this directory to find a
reviewer. If no OWNERS file exists, please consider signing up at
go/reviewing-metrics (Googlers only), as all subdirectories are expected to
have an OWNERS file. As a last resort you can send the CL to
chromium-metrics-reviews@google.com.
-->

<histogram-configuration>

<!-- Enum types -->

<enums>


</enums>

</histogram-configuration>
"""

ENUMS_PATH = path_util.GetInputFile('tools/metrics/histograms/enums.xml')


def _get_enums_from_files(files):
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


def _extract_enum_nodes_by_names(enum_names):
  """Returns the <enum> nodes corresponding to the specified names."""
  with io.open(ENUMS_PATH, 'r', encoding='utf-8') as f:
    document = minidom.parse(f)

  enum_nodes = []
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value in enum_names:
      enum_nodes.append(enum_node)

  for node in enum_nodes:
    node.parentNode.removeChild(node)

  xml_with_nodes_removed = histogram_configuration_model.PrettifyTree(document)
  return enum_nodes, xml_with_nodes_removed


def _read_enums_xml_or_blank_template(path):
  """Reads the enums XML file as minidom or a blank template if not found."""
  if not os.path.isfile(path):
    print(f'No existing file at {path}, creating it.')
    return minidom.parseString(ENUMS_XML_TEMPLATE)
  with io.open(path, 'r', encoding='utf-8') as f:
    print(f'Oppening existing file {path}...')
    return minidom.parse(f)


def _move_enums_to_file(enum_nodes, enums_file):
  """Adds enum nodes to `enums_file` and returns the updated XML."""
  document = _read_enums_xml_or_blank_template(enums_file)
  enums_node = document.getElementsByTagName('enums')[0]
  for node in enum_nodes:
    enums_node.appendChild(document.importNode(node, True))
  return histogram_configuration_model.PrettifyTree(document)


def _split_enums(dir_name):
  """Splits out an enums.xml file in the specified directory."""
  histograms_file = path_util.GetInputFile(
      f'tools/metrics/histograms/metadata/{dir_name}/histograms.xml')
  if not os.path.isfile(histograms_file):
    print(f'File {histograms_file} not found! Exiting.')
    return

  enums_file = path_util.GetInputFile(
      f'tools/metrics/histograms/metadata/{dir_name}/enums.xml')

  print(f'Reading XML files...')

  # Get the enums referenced by the given histograms.xml file.
  relevant_files = [histograms_file, ENUMS_PATH]
  if os.path.isfile(enums_file):
    relevant_files.append(enums_file)
  enum_names = _get_enums_from_files(relevant_files)

  # Only move enums that aren't referenced by other files.
  all_enum_names = _get_enums_from_files(
      [f for f in histogram_paths.ALL_XMLS if f != histograms_file])
  candidate_enum_names = enum_names - all_enum_names
  print(f'Found {len(candidate_enum_names)} candidate enums.')

  enum_nodes, updated_full_xml = _extract_enum_nodes_by_names(
      candidate_enum_names)
  # There may be fewer, when some of the enums are not in the common enums.xml
  # file (for example, they may be in the target file already).
  assert len(enum_nodes) <= len(candidate_enum_names)
  print(f'Of these, {len(enum_nodes)} are in the common enums.xml file.')

  new_enums_xml = _move_enums_to_file(enum_nodes, enums_file)

  enums_file_did_not_exist = not os.path.isfile(enums_file)
  print(f'Writing updated file: {enums_file}')
  with open(enums_file, 'w', encoding='utf-8', newline='') as f:
    f.write(new_enums_xml)
  print(f'Writing updated file: {ENUMS_PATH}')
  with open(ENUMS_PATH, 'w', encoding='utf-8', newline='') as f:
    f.write(updated_full_xml)

  print('')
  print(f'Moved {len(enum_nodes)} to {enums_file}.')
  print('')

  if enums_file_did_not_exist:
    print('Please run `git add` on the new enums.xml file:')
    print(f'  git add {enums_file}')
    print('')
    print('Updating histograms_xml_files.gni file...')
    histogram_paths.UpdateHistogramsXmlGniFile()
    print('Done.')


def main():
  logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)
  if len(sys.argv) != 2:
    print('Usage split_enums.py <dir_name>')
    return
  _split_enums(sys.argv[1])


if __name__ == '__main__':
  main()
