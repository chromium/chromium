# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Splits a XML file into smaller XMLs in subfolders.

Splits nodes according to the first camelcase part of their name attribute.
Intended to be used to split up the large histograms.xml or enums.xml file.
"""

import os
import re
from xml.dom import minidom

import histogram_configuration_model
import histogram_paths
import merge_xml
import path_util

# The top level comment templates that will be formatted and added to each split
# histograms xml.
FIRST_TOP_LEVEL_COMMENT_TEMPLATE = """
Copyright 2021 The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
"""
SECOND_TOP_LEVEL_COMMENT_TEMPLATE = """
This file is used to generate a comprehensive list of %s
along with a detailed description for each histogram.

For best practices on writing histogram descriptions, see
https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md

Please send CLs to individuals in the OWNERS file in the same directory as this
xml file. If no OWNERS file exists, then send the CL to
chromium-metrics-reviews@google.com.
"""
# Number of times that splitting of histograms will be carried out.
TARGET_DEPTH = 1
# The number of histograms below which they will be aggregated into
# the histograms.xml in 'others'.
AGGREGATE_THRESHOLD = 20
# A map from the histogram name to the folder name these histograms should be
# put in.
_PREDEFINED_NAMES_MAPPING = {
    'BackForwardCache': 'BackForwardCache',
    'ChromeOS': 'ChromeOS',
    'CustomTabs': 'CustomTabs',
    'CustomTab': 'CustomTabs',
    'DataReductionProxy': 'DataReductionProxy',
    'DataUse': 'DataUse',
    'MultiDevice': 'MultiDevice',
    'NaCl': 'NaCl',
    'SafeBrowsing': 'SafeBrowsing',
    'SafeBrowsingBinaryUploadRequest': 'SafeBrowsing',
    'SafeBrowsingFCMService': 'SafeBrowsing',
    'NewTabPage': 'NewTabPage',
    'SiteEngagementService': 'SiteEngagementService',
    'SiteIsolation': 'SiteIsolation',
    'Tabs': 'Tab',
    'TextFragmentAnchor': 'TextFragmentAnchor',
    'TextToSpeech': 'TextToSpeech',
    'UpdateEngine': 'UpdateEngine',
    'WebApk': 'WebApk',
    'WebApp': 'WebApp',
    'WebAudio': 'WebAudio',
    'WebAuthentication': 'WebAuthentication',
    'WebCore': 'WebCore',
    'WebFont': 'WebFont',
    'WebHistory': 'WebHistory',
    'WebRTC': 'WebRTC',
    'WebRtcEventLogging': 'WebRTC',
    'WebRtcTextLogging': 'WebRTC',
    'WebUI': 'WebUI',
    'WebUITabStrip': 'WebUI',
}


def _ParseMergedXML():
  """Parses merged xml into different types of nodes"""
  merged_histograms = merge_xml.MergeFiles(histogram_paths.HISTOGRAMS_XMLS)
  histogram_nodes = merged_histograms.getElementsByTagName('histogram')
  variants_nodes = merged_histograms.getElementsByTagName('variants')
  histogram_suffixes_nodes = merged_histograms.getElementsByTagName(
      'histogram_suffixes')
  return histogram_nodes, variants_nodes, histogram_suffixes_nodes


def _CreateXMLFile(comment, parent_node_string, nodes, output_dir, filename):
  """Creates XML file for given type of XML nodes.

  This function also creates a |parent_node_string| tag as the parent node, e.g.
  <histograms> or <histogram_suffixes_list>, that wraps all the |nodes| in the
  output XML.

  Args:
    comment: The string to be formatted in the |TOP_LEVEL_COMMENT_TEMPLATE|
        which will then be added on top of each split xml.
    parent_node_string: The name of the the second-level parent node, e.g.
        <histograms> or <histogram_suffixes_list>.
    nodes: A DOM NodeList object or a list containing <histogram> or
        <histogram_suffixes> that will be inserted under the parent node.
    output_dir: The output directory.
    filename: The output filename.
  """
  doc = minidom.Document()

  doc.appendChild(doc.createComment(FIRST_TOP_LEVEL_COMMENT_TEMPLATE))
  doc.appendChild(doc.createComment(SECOND_TOP_LEVEL_COMMENT_TEMPLATE %
                                    comment))

  # Create the <histogram-configuration> node for the new histograms.xml file.
  histogram_config_element = doc.createElement('histogram-configuration')
  doc.appendChild(histogram_config_element)
  parent_element = doc.createElement(parent_node_string)
  histogram_config_element.appendChild(parent_element)

  # Under the parent node, append the children nodes.
  for node in nodes:
    parent_element.appendChild(node)

  output_path = os.path.join(output_dir, filename)
  if os.path.exists(output_path):
    os.remove(output_path)

  # Use the model to get pretty-printed XML string and write into file.
  with open(output_path, 'w') as output_file:
    pretty_xml_string = histogram_configuration_model.PrettifyTree(doc)
    output_file.write(pretty_xml_string)


def _GetCamelCaseName(node, depth=0):
  """Returns the first camelcase name part of the given |node|.

  Args:
    node: The node to get name from.
    depth: The depth that specifies which name part will be returned.
        e.g. For a node of name
        'CustomTabs.DynamicModule.CreatePackageContextTime'
        The returned camel name for depth 0 is 'Custom';
        The returned camel name for depth 1 is 'Dynamic';
        The returned camel name for depth 2 is 'Create'.

        Default depth is set to 0 as this function is imported and
        used in other files, where depth used is 0.

  Returns:
    The camelcase name part at specified depth. If the number of name parts is
    less than the depth, return 'others'.
  """
  name = node.getAttribute('name')
  split_string_list = name.split('.')
  if len(split_string_list) <= depth:
    return 'others'
  elif split_string_list[depth] in _PREDEFINED_NAMES_MAPPING:
    return _PREDEFINED_NAMES_MAPPING[split_string_list[depth]]
  else:
    name_part = split_string_list[depth]
    start_index = 0
    # |all_upper| is used to identify the case where the name is ABCDelta, in
    # which case the camel name of depth 0 should be ABC, instead of A.
    all_upper = True
    for index, letter in enumerate(name_part):
      if letter.islower() or letter.isnumeric():
        all_upper = False
      if letter.isupper() and not all_upper:
        start_index = index
        break

  if start_index == 0:
    return name_part
  else:
    return name_part[0:start_index]


def GetDirForNode(node):
  """Returns the correct directory that the given |node| should be placed in."""
  camel_name = _GetCamelCaseName(node)
  # Check if the directory of its prefix exists. Return the |camel_name| if the
  # folder exists. Otherwise, this |node| should be placed in 'others' folder.
  if camel_name in histogram_paths.HISTOGRAMS_PREFIX_LIST:
    return camel_name
  return 'others'


def _CamelCaseToSnakeCase(name):
  """Converts CamelCase |name| to snake_case."""
  name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def _OutputToFolderAndXML(nodes, output_dir, key):
  """Creates new folder and XML file for separated histograms.

  Args:
    nodes: A list of histogram/variants nodes of a prefix.
    output_dir: The output directory.
    key: The prefix of the histograms, also the name of the new folder.
  """
  # Convert CamelCase name to snake_case when creating a directory.
  output_dir = os.path.join(output_dir, _CamelCaseToSnakeCase(key))
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  _CreateXMLFile(key + ' histograms', 'histograms', nodes, output_dir,
                 'histograms.xml')


def _WriteDocumentDict(document_dict, output_dir):
  """Recursively writes |document_dict| to xmls in |output_dir|.

  Args:
    document_dict: A dictionary where the key is the prefix of the histogram and
        value is a list of nodes or another dict.
    output_dir: The output directory of the resulting folders.
  """
  for key, val in document_dict.items():
    if isinstance(val, list):
      _OutputToFolderAndXML(val, output_dir, key)
    else:
      _WriteDocumentDict(val, os.path.join(output_dir, key))


def _AggregateMinorNodes(node_dict):
  """Aggregates groups of nodes below threshold number into 'others'.

  Args:
    node_dict: A dictionary where the key is the prefix of the histogram/variant
        and value is a list of histogram/variant nodes.
  """
  others = node_dict.pop('others', [])

  for key, nodes in node_dict.items():
    # For a prefix, if the number of histograms is fewer than threshold,
    # aggregate into others.
    if len(nodes) < AGGREGATE_THRESHOLD:
      others.extend(nodes)
      del node_dict[key]

  if others:
    node_dict['others'] = others


def _BuildDocumentDict(nodes, depth):
  """Recursively builds a document dict which will be written later.

  This function recursively builds a document dict which the key of the dict is
  the first word of the node's name at the given |depth| and the value of the
  dict is either a list of nodes that correspond to the key or another dict if
  it doesn't reach to |TARGET_DEPTH|.

  Args:
    nodes: A list of histogram nodes or variants node.
    depth: The current depth, starting from 0.

  Returns:
    The document dict.
  """
  if depth == TARGET_DEPTH:
    return nodes

  temp_dict = document_dict = {}
  for node in nodes:
    name_part = _GetCamelCaseName(node, depth)
    if name_part not in temp_dict:
      temp_dict[name_part] = []
    temp_dict[name_part].append(node)

  # Aggregate keys with less than |AGGREGATE_THRESHOLD| values to 'others'.
  _AggregateMinorNodes(temp_dict)

  for key, nodes in temp_dict.items():
    if key == 'others':
      document_dict[key] = nodes
    else:
      document_dict[key] = _BuildDocumentDict(nodes, depth + 1)

  return document_dict


def SplitIntoMultipleHistogramXMLs(output_base_dir):
  """Splits a large histograms.xml and writes out the split xmls.

  Args:
    output_base_dir: The output base directory.
  """
  if not os.path.exists(output_base_dir):
    os.mkdir(output_base_dir)

  histogram_nodes, variants_nodes, histogram_suffixes_nodes = _ParseMergedXML()

  # Create separate XML file for histogram suffixes.
  _CreateXMLFile('histogram suffixes', 'histogram_suffixes_list',
                 histogram_suffixes_nodes, output_base_dir,
                 'histogram_suffixes_list.xml')
  document_dict = _BuildDocumentDict(histogram_nodes + variants_nodes, 0)

  _WriteDocumentDict(document_dict, output_base_dir)


if __name__ == '__main__':
  SplitIntoMultipleHistogramXMLs(
      path_util.GetInputFile('tools/metrics/histograms/metadata'))
