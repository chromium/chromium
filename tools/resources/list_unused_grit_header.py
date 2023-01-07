#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to scan source files for unneeded grit includes.

Example:
  cd /work/chrome/src
  tools/resources/list_unused_grit_header.py ui/strings/ui_strings.grd chrome ui
"""

from __future__ import print_function

import os
import sys
import xml.etree.ElementTree

from find_unused_resources import GetBaseResourceId

IF_ELSE_THEN_TAGS = ('if', 'else', 'then')


def Usage(prog_name):
  print(prog_name, 'GRD_FILE PATHS_TO_SCAN')


def FilterResourceIds(resource_id):
  """If the resource starts with IDR_, find its base resource id."""
  if resource_id.startswith('IDR_'):
    return GetBaseResourceId(resource_id)
  return resource_id


def GetResourcesForNode(node, parent_file, resource_tag):
  """Recursively iterate through a node and extract resource names.

  Args:
    node: The node to iterate through.
    parent_file: The file that contains node.
    resource_tag: The resource tag to extract names from.

  Returns:
    A list of resource names.
  """
  resources = []
  for child in node.getchildren():
    if child.tag == resource_tag:
      resources.append(child.attrib['name'])
    elif child.tag in IF_ELSE_THEN_TAGS:
      resources.extend(GetResourcesForNode(child, parent_file, resource_tag))
    elif child.tag == 'part':
      parent_dir = os.path.dirname(parent_file)
      part_file = os.path.join(parent_dir, child.attrib['file'])
      part_tree = xml.etree.ElementTree.parse(part_file)
      part_root = part_tree.getroot()
      assert part_root.tag == 'grit-part'
      resources.extend(GetResourcesForNode(part_root, part_file, resource_tag))
    else:
      raise Exception('unknown tag:', child.tag)

  # Handle the special case for resources of type "FOO_{LEFT,RIGHT,TOP}".
  if resource_tag == 'structure':
    resources = [FilterResourceIds(resource_id) for resource_id in resources]
  return resources


def FindNodeWithTag(node, tag):
  """Look through a node's children for a child node with a given tag.

  Args:
    root: The node to examine.
    tag: The tag on a child node to look for.

  Returns:
    A child node with the given tag, or None.
  """
  result = None
  for n in node.getchildren():
    if n.tag == tag:
      assert not result
      result = n
  return result


def GetResourcesForGrdFile(tree, grd_file):
  """Find all the message and include resources from a given grit file.

  Args:
    tree: The XML tree.
    grd_file: The file that contains the XML tree.

  Returns:
    A list of resource names.
  """
  root = tree.getroot()
  assert root.tag == 'grit'
  release_node = FindNodeWithTag(root, 'release')
  assert release_node != None

  resources = set()
  for node_type in ('message', 'include', 'structure'):
    resources_node = FindNodeWithTag(release_node, node_type + 's')
    if resources_node != None:
      resources = resources.union(
          set(GetResourcesForNode(resources_node, grd_file, node_type)))
  return resources


def GetOutputFileForNode(node):
  """Find the output file starting from a given node.

  Args:
    node: The root node to scan from.

  Returns:
    A grit header file name.
  """
  output_file = None
  for child in node.getchildren():
    if child.tag == 'output':
      if child.attrib['type'] == 'rc_header':
        assert output_file is None
        output_file = child.attrib['filename']
    elif child.tag in IF_ELSE_THEN_TAGS:
      child_output_file = GetOutputFileForNode(child)
      if not child_output_file:
        continue
      assert output_file is None
      output_file = child_output_file
    else:
      raise Exception('unknown tag:', child.tag)
  return output_file


def GetOutputHeaderFile(tree):
  """Find the output file for a given tree.

  Args:
    tree: The tree to scan.

  Returns:
    A grit header file name.
  """
  root = tree.getroot()
  assert root.tag == 'grit'
  output_node = FindNodeWithTag(root, 'outputs')
  assert output_node != None
  return GetOutputFileForNode(output_node)


def ShouldScanFile(filename):
  """Return if the filename has one of the extensions below."""
  extensions = ['.cc', '.cpp', '.h', '.mm']
  file_extension = os.path.splitext(filename)[1]
  return file_extension in extensions


def NeedsGritInclude(grit_header, resources, filename):
  """Return whether a file needs a given grit header or not.

  Args:
    grit_header: The grit header file name.
    resources: The list of resource names in grit_header.
    filename: The file to scan.

  Returns:
    True if the file should include the grit header.
  """
  # A list of special keywords that implies the file needs grit headers.
  # To be more thorough, one would need to run a pre-processor.
  SPECIAL_KEYWORDS = (
      '#include "ui_localizer_table.h"',  # ui_localizer.mm
      'DECLARE_RESOURCE_ID',  # chrome/browser/android/resource_mapper.cc
      )
  with open(filename, 'rb') as f:
    grit_header_line = grit_header + '"\n'
    has_grit_header = False
    while True:
      line = f.readline()
      if not line:
        break
      if line.endswith(grit_header_line):
        has_grit_header = True
        break

    if not has_grit_header:
      return True
    rest_of_the_file = f.read()
    return (any(resource in rest_of_the_file for resource in resources) or
            any(keyword in rest_of_the_file for keyword in SPECIAL_KEYWORDS))


def main(argv):
  if len(argv) < 3:
    Usage(argv[0])
    return 1
  grd_file = argv[1]
  paths_to_scan = argv[2:]
  for f in paths_to_scan:
    if not os.path.exists(f):
      print('Error: %s does not exist' % f)
      return 1

  tree = xml.etree.ElementTree.parse(grd_file)
  grit_header = GetOutputHeaderFile(tree)
  if not grit_header:
    print('Error: %s does not generate any output headers.' % grd_file)
    return 1

  resources = GetResourcesForGrdFile(tree, grd_file)

  files_with_unneeded_grit_includes = []
  for path_to_scan in paths_to_scan:
    if os.path.isdir(path_to_scan):
      for root, dirs, files in os.walk(path_to_scan):
        if '.git' in dirs:
          dirs.remove('.git')
        full_paths = [os.path.join(root, f) for f in files if ShouldScanFile(f)]
        files_with_unneeded_grit_includes.extend(
            [f for f in full_paths
             if not NeedsGritInclude(grit_header, resources, f)])
    elif os.path.isfile(path_to_scan):
      if not NeedsGritInclude(grit_header, resources, path_to_scan):
        files_with_unneeded_grit_includes.append(path_to_scan)
    else:
      print('Warning: Skipping %s' % path_to_scan)

  if files_with_unneeded_grit_includes:
    print('\n'.join(files_with_unneeded_grit_includes))
    return 2
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
