#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to merge multiple source xml files into a single histograms.xml."""

import argparse
import copy
import os
import xml.dom.minidom
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.xml_utils as xml_utils
import chromium_src.tools.metrics.histograms.expand_owners as expand_owners
import chromium_src.tools.metrics.histograms.histogram_configuration_model as histogram_configuration_model
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.populate_enums as populate_enums


def GetElementsByTagName(trees, tag, depth=2):
  """Gets all elements with the specified tag from a set of ET trees.

  Args:
    trees: A list of ET elements.
    tag: The tag of the elements to find.
    depth: The depth in the trees by which a match should be found.

  Returns:
    A list of ET elements with the specified tag.
  """
  iterator = xml_utils.IterElementsWithTag
  return list(e for t in trees for e in iterator(t, tag, depth))


def CombineEnumsSections(trees):
  """Combines multiple <enums> from the passed in ET trees into one.

  If trees contain ukm events, populates a list of ints to the
  "UkmEventNameHash" enum where each value is a ukm event name hash truncated
  to 31 bits and each label is the corresponding event name.

  Args:
    trees: A list of ET trees.

  Returns:
    A single <enums> ET Element.
  """
  enums_node = ET.Element('enums')
  # Pass depth=3 as default depth=2 won't find enum tags that are 3 levels deep.
  for enum in GetElementsByTagName(trees, 'enum', depth=3):
    enums_node.append(copy.deepcopy(enum))

  ukm_events = GetElementsByTagName(
    GetElementsByTagName(trees, 'ukm-configuration'), 'event'
  )
  if ukm_events:
    populate_enums.PopulateEnumsWithUkmEvents(enums_node, ukm_events)
  return enums_node


def CombineHistogramsSorted(trees):
  """Sorts histograms related nodes by name and returns the combined nodes.

  This function sorts nodes including <histogram>, <variant> and
  <histogram_suffix>. Then it returns one <histograms> that contains the
  sorted <histogram> and <variant> nodes and the other <histogram_suffixes_list>
  node containing all <histogram_suffixes> nodes.

  Args:
    trees: A list of ET trees.

  Returns:
    A list containing the combined <histograms> Element and the combined
    <histogram_suffixes_list> Element.
  """
  combined_histograms = ET.Element('histograms')

  def SortByLowerCaseName(node):
    return node.get('name').lower()

  variants_nodes = GetElementsByTagName(trees, 'variants', depth=3)
  sorted_variants = sorted(variants_nodes, key=SortByLowerCaseName)

  histogram_nodes = GetElementsByTagName(trees, 'histogram', depth=3)
  sorted_histograms = sorted(histogram_nodes, key=SortByLowerCaseName)

  for variants in sorted_variants:
    combined_histograms.append(copy.deepcopy(variants))

  for histogram in sorted_histograms:
    combined_histograms.append(copy.deepcopy(histogram))

  # Create the combined <histogram_suffixes_list> tag.
  combined_histogram_suffixes_list = ET.Element('histogram_suffixes_list')

  histogram_suffixes_nodes = GetElementsByTagName(
    trees, 'histogram_suffixes', depth=3
  )
  sorted_histogram_suffixes = sorted(
    histogram_suffixes_nodes, key=SortByLowerCaseName
  )

  for histogram_suffixes in sorted_histogram_suffixes:
    combined_histogram_suffixes_list.append(copy.deepcopy(histogram_suffixes))

  return [combined_histograms, combined_histogram_suffixes_list]


def MergeTrees(
  trees: list[ET.Element], should_expand_owners: bool = True
) -> ET.Element:
  """Merges a list of histograms.xml ET trees.

  Args:
    trees: A list of histograms.xml ET trees.
    should_expand_owners: Whether we want to expand owners for histograms.

  Returns:
  Returns:
    A merged ET Element.
  """
  root = ET.Element('histogram-configuration')
  root.append(CombineEnumsSections(trees))
  for node in CombineHistogramsSorted(trees):
    root.append(node)
  if should_expand_owners:
    expand_owners.ExpandHistogramsOWNERS(root)
  return root


# TODO(crbug.com/531790306): Deprecated. All callers of MergeTreesDeprecated
# should be migrated to MergeTrees (ElementTree version), and this function
# should be removed.
def MergeTreesDeprecated(trees, should_expand_owners=True):
  """Deprecated. Merges a list of DOM trees and returns a DOM Document."""
  et_trees = [ET.fromstring(t.toxml()) for t in trees]
  merged_et = MergeTrees(et_trees, should_expand_owners)
  xml_string = ET.tostring(merged_et, encoding='utf-8')
  return xml.dom.minidom.parseString(xml_string)


def _AddComponentFromMetadataFile(
  root: ET.Element, metadata_filename: str
) -> ET.Element:
  """Adds the component from the metadata file to the ET tree."""
  component = expand_owners.ExtractComponentViaDirmd(
    os.path.dirname(metadata_filename)
  )
  if not component:
    return root

  for histograms in xml_utils.IterElementsWithTag(root, 'histograms', 2):
    for histogram in xml_utils.IterElementsWithTag(histograms, 'histogram', 1):
      component_element = ET.Element('component')
      component_element.text = component
      histogram.append(component_element)
  return root


def _BuildTreeWithComponentMetadata(filename_or_file):
  """Builds the ET tree for the given file.

  Args:
    filename_or_file: The string filename or the file handle for histograms.xml.

  Returns:
    The histograms.xml ET tree with (optional) component metadata.
  """
  root = ET.parse(filename_or_file).getroot()

  if isinstance(filename_or_file, str):
    metadata_filename = os.path.join(
      os.path.dirname(filename_or_file), 'DIR_METADATA'
    )
    if os.path.exists(metadata_filename):
      return _AddComponentFromMetadataFile(root, metadata_filename)
  return root


def MergeFiles(
  filenames=[], files=[], expand_owners_and_extract_components=False
):
  """Merges a list of histograms.xml files using ElementTree.

  Args:
    filenames: A list of histograms.xml filenames.
    files: A list of histograms.xml file-like objects.
    expand_owners_and_extract_components: Whether we want to expand owners and
      extract components.

  Returns:
    A merged ElementTree Element.
  """
  all_files = files + filenames
  if expand_owners_and_extract_components:
    trees = [_BuildTreeWithComponentMetadata(f) for f in all_files]
  else:
    trees = xml_utils.ParseXMLFiles(all_files)

  merged_et = MergeTrees(
    trees, should_expand_owners=expand_owners_and_extract_components
  )

  return merged_et


# TODO(crbug.com/531790306): Deprecated. All callers of MergeFilesDeprecated
# should be migrated to MergeFiles (ElementTree version), and this function
# should be removed.
def MergeFilesDeprecated(
  filenames=[], files=[], expand_owners_and_extract_components=False
):
  """Deprecated. Merges a list of XML files and returns a minidom Document."""
  if expand_owners_and_extract_components:
    all_files = files + filenames
    trees = [_BuildTreeWithComponentMetadata(f) for f in all_files]
    merged_et = MergeTrees(trees)
    expand_owners.ExpandHistogramsOWNERS(merged_et)
  else:
    merged_et = MergeFiles(filenames, files)

  # Convert to minidom.Document for backward compatibility.
  xml_string = ET.tostring(merged_et, encoding='utf-8')
  doc = xml.dom.minidom.parseString(xml_string)
  return doc


def PrettyPrintMergedFiles(filenames=[], files=[]):
  return histogram_configuration_model.PrettifyTree(
    MergeFiles(
      filenames=filenames,
      files=files,
      expand_owners_and_extract_components=True,
    )
  )


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', required=True)
  args = parser.parse_args()
  with open(args.output, 'w', encoding='utf-8', newline='\n') as f:
    # This is run by
    # https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/BUILD.gn;drc=573e48309695102dec2da1e8f806c18c3200d414;l=5
    # to send the merged histograms.xml to the server side. Providing |UKM_XML|
    # here is not to merge ukm.xml but to populate `UkmEventNameHash` enum
    # values.
    f.write(
      PrettyPrintMergedFiles(
        histogram_paths.ALL_XMLS + [histogram_paths.UKM_XML]
      )
    )


if __name__ == '__main__':
  main()
