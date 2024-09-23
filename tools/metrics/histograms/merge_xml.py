#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to merge multiple source xml files into a single histograms.xml."""

import argparse
import os
import sys
import xml.dom.minidom

import expand_owners
import histogram_configuration_model
import histogram_paths
import populate_enums
import xml_utils


def GetElementsByTagName(trees, tag, depth=2):
  """Gets all elements with the specified tag from a set of DOM trees.

  Args:
    trees: A list of DOM trees.
    tag: The tag of the elements to find.
    depth: The depth in the trees by which a match should be found.

  Returns:
    A list of DOM nodes with the specified tag.
  """
  iterator = xml_utils.IterElementsWithTag
  return list(e for t in trees for e in iterator(t, tag, depth))


def CombineEnumsSections(doc, trees):
  """Combines multiple <enums> from the passed in DOM trees into one.

  If trees contain ukm events, populates a list of ints to the
  "UkmEventNameHash" enum where each value is a ukm event name hash truncated
  to 31 bits and each label is the corresponding event name.

  Args:
    doc: The document where the new single <enums> section will be created.
    trees: A list of DOM trees.

  Returns:
    A single <enums> DOM node.
  """
  enums_node = doc.createElement('enums')
  # Pass depth=3 as default depth=2 won't find enum tags that are 3 levels deep.
  for enum in GetElementsByTagName(trees, 'enum', depth=3):
    xml.dom.minidom._append_child(enums_node, enum)

  ukm_events = GetElementsByTagName(
      GetElementsByTagName(trees, 'ukm-configuration'), 'event')
  if ukm_events:
    populate_enums.PopulateEnumsWithUkmEvents(doc, enums_node, ukm_events)
  return enums_node


def CombineHistogramsSorted(doc, trees):
  """Sorts histograms related nodes by name and returns the combined nodes.

  This function sorts nodes including <histogram>, <variant> and
  <histogram_suffix>. Then it returns one <histograms> that contains the
  sorted <histogram> and <variant> nodes and the other <histogram_suffixes_list>
  node containing all <histogram_suffixes> nodes.

  Args:
    doc: The document to create the node in.
    trees: A list of DOM trees.

  Returns:
    A list containing the combined <histograms> node and the combined
    <histogram_suffix_list> node.
  """
  # Create the combined <histograms> tag.
  combined_histograms = doc.createElement('histograms')

  def SortByLowerCaseName(node):
    return node.getAttribute('name').lower()

  variants_nodes = GetElementsByTagName(trees, 'variants', depth=3)
  sorted_variants = sorted(variants_nodes, key=SortByLowerCaseName)

  histogram_nodes = GetElementsByTagName(trees, 'histogram', depth=3)
  sorted_histograms = sorted(histogram_nodes, key=SortByLowerCaseName)

  for variants in sorted_variants:
    # Use unsafe version of `appendChild` function here because the safe one
    # takes a lot longer (10000x) to append all children. The unsafe version
    # is ok here because:
    #   1. the node to be appended is a clean node.
    #   2. The unsafe version only does fewer checks but not changing any
    #     behavior and it's documented to be usable if performance matters.
    #     See https://github.com/python/cpython/blob/2.7/Lib/xml/dom/minidom.py#L276.
    xml.dom.minidom._append_child(combined_histograms, variants)

  for histogram in sorted_histograms:
    xml.dom.minidom._append_child(combined_histograms, histogram)

  # Create the combined <histogram_suffixes_list> tag.
  combined_histogram_suffixes_list = doc.createElement(
      'histogram_suffixes_list')

  histogram_suffixes_nodes = GetElementsByTagName(trees,
                                                  'histogram_suffixes',
                                                  depth=3)
  sorted_histogram_suffixes = sorted(histogram_suffixes_nodes,
                                     key=SortByLowerCaseName)

  for histogram_suffixes in sorted_histogram_suffixes:
    xml.dom.minidom._append_child(combined_histogram_suffixes_list,
                                  histogram_suffixes)

  return [combined_histograms, combined_histogram_suffixes_list]


def MakeNodeWithChildren(doc, tag, children):
  """Creates a DOM node with specified tag and child nodes.

  Args:
    doc: The document to create the node in.
    tag: The tag to create the node with.
    children: A list of DOM nodes to add as children.

  Returns:
    A DOM node.
  """
  node = doc.createElement(tag)
  for child in children:
    node.appendChild(child)
  return node


def MergeTrees(trees, should_expand_owners):
  """Merges a list of histograms.xml DOM trees.

  Args:
    trees: A list of histograms.xml DOM trees.
    should_expand_owners: Whether we want to expand owners for histograms.

  Returns:
    A merged DOM tree.
  """
  doc = xml.dom.minidom.Document()
  doc.appendChild(
      MakeNodeWithChildren(
          doc,
          'histogram-configuration',
          [CombineEnumsSections(doc, trees)] +
          # Sort the <histogram> and <histogram_suffixes> nodes by name and
          # return the combined nodes.
          CombineHistogramsSorted(doc, trees)))
  # After using the unsafe version of appendChild, we see a regression when
  # pretty-printing the merged |doc|. This might because the unsafe appendChild
  # doesn't build indexes for later lookup. And thus, we need to convert the
  # merged |doc| to a xml string and convert it back to force it to build
  # indexes for the merged |doc|.
  doc = xml.dom.minidom.parseString(doc.toxml().encode('utf-8'))
  # Only perform fancy operations after |doc| becomes stable. This helps improve
  # the runtime performance.
  if should_expand_owners:
    for histograms in doc.getElementsByTagName('histograms'):
      expand_owners.ExpandHistogramsOWNERS(histograms)
  return doc


def _AddComponentFromMetadataFile(tree, filename):
  """Adds the component from the metadata file to the DOM tree.

  Args:
    tree: A histogram.xml DOM tree.
    filename: The name of the metadata file.

  Returns:
    The updated tree with the component (optionally) added.
  """
  component = expand_owners.ExtractComponentViaDirmd(os.path.dirname(filename))
  if component:
    histograms = tree.getElementsByTagName('histograms')
    if histograms:
      iter_matches = xml_utils.IterElementsWithTag
      for histogram in iter_matches(histograms[0], 'histogram'):
        expand_owners.AddHistogramComponent(histogram, component)
  return tree


def _BuildDOMTreeWithComponentMetadata(filename_or_file):
  """Builds the DOM tree for the given file.

  Args:
    filename_or_file: The string filename or the file handle for histograms.xml.

  Returns:
    The histograms.xml DOM tree with (optional) component metadata.
  """
  tree = xml.dom.minidom.parse(filename_or_file)
  if isinstance(filename_or_file, str):
    # If we can find a metadata file in the same directory, we try to extract
    # a component from it.
    metadata_filename = os.path.join(os.path.dirname(filename_or_file),
                                     'DIR_METADATA')
    if os.path.exists(metadata_filename):
      return _AddComponentFromMetadataFile(tree, metadata_filename)
  return tree


def MergeFiles(filenames=[],
               files=[],
               expand_owners_and_extract_components=False):
  """Merges a list of histograms.xml files.

  Args:
    filenames: A list of histograms.xml filenames.
    files: A list of histograms.xml file-like objects.
    expand_owners_and_extract_components: Whether we want to expand owners and
      extract components. By default, it's false because most of the callers
      don't care about the owners or components for each metadata.

  Returns:
    A merged DOM tree.
  """
  # minidom.parse() takes both files and filenames:
  all_files = files + filenames
  trees = [
      _BuildDOMTreeWithComponentMetadata(f)
      if expand_owners_and_extract_components else xml.dom.minidom.parse(f)
      for f in all_files
  ]
  return MergeTrees(trees,
                    should_expand_owners=expand_owners_and_extract_components)


def PrettyPrintMergedFiles(filenames=[], files=[]):
  return histogram_configuration_model.PrettifyTree(
      MergeFiles(filenames=filenames,
                 files=files,
                 expand_owners_and_extract_components=True))


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
    f.write(PrettyPrintMergedFiles(
      histogram_paths.ALL_XMLS + [histogram_paths.UKM_XML]))


if __name__ == '__main__':
  main()
