#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to extract edits from rewrite_templated_container_fields clang
tool output.

If the tool emits edits, then the edits should look like this:
    ...
    {lhs_node1};{lhs_node2}
    {node_n}
    {lhs_node3};{lhs_node4}
    ...
    ...
Where lhs_node, rhs_node, and node_n represent a node's text representation
generated using rewrite_templated_container_fields' Node::ToString() function.

The string representation has the following format:
`{is_field\,is_excluded\,has_auto_type\,r:::<file path>:::<offset>:::<length>
:::<replacement text>\,include-user-header:::<file path>:::-1:::-1
:::<include text>}`

where `is_field`,`is_excluded`, and `has_auto_type` are booleans represendted
as  0 or 1.

extract_edits.py takes input that is concatenated from multiple tool
invocations and extract just the edits with the following steps:
1- Construct the adjacency list of nodes
   (a pairs of nodes represents an edge in the graph)

2- Run `PropagateExclusions` to exclude fields reachable
   from a RAW_PTR_EXCLUSION annotated field.

3- Run `DFS` starting from non-excluded nodes and emit
   edtis for reachable nodes.

extract_edits.py would then emit the following output:
    <edit1>
    <edit2>
    <edit3>
    ...
Where the edit is either a replacemnt or an include directive.

For more details about how the tool works, see the doc here:
https://docs.google.com/document/d/1P8wLVS3xueI4p3EAPO4JJP6d1_zVp5SapQB0EW9iHQI/
"""

from __future__ import print_function
from collections import defaultdict
import sys


class Node:
  is_field = "0"
  is_excluded = "0"
  has_auto_type = "0"
  replacement = ""
  include_directive = ""
  neighbors = set()

  def __init__(self, is_field, is_excluded, has_auto_type, replacement,
               include_directive) -> None:
    self.is_field = is_field
    self.is_excluded = is_excluded
    self.replacement = replacement
    self.has_auto_type = has_auto_type
    self.include_directive = include_directive
    self.neighbors = set()

  def __eq__(self, other):
    if isinstance(other, Node):
      return self.replacement == other.replacement
    return False

  def __hash__(self) -> int:
    return hash((self.replacement, self.include_directive))


def GetNode(txt: str):
  txt = txt[1:len(txt) - 1]
  x = txt.split('\\,')
  return Node(x[0], x[1], x[2], x[3], x[4])


def DFS(visited: set, graph: defaultdict, key: str, key_to_node: defaultdict,
        changes: set):
  if key not in visited:
    node = key_to_node[key]
    if node.has_auto_type == "0":
      changes.add(node.replacement)
      changes.add(node.include_directive)
    visited.add(key)
    for neighbour in graph[key]:
      DFS(visited, graph, neighbour.replacement, key_to_node, changes)


# to propagate field exclusions to all neighbors
def PropagateExclusions(visited: set, graph: defaultdict, key: str,
                        key_to_node: defaultdict):
  if key not in visited:
    n = key_to_node[key]
    n.is_excluded = "1"
    key_to_node[key] = n
    visited.add(key)
    for neighbour in graph[key]:
      PropagateExclusions(visited, graph, neighbour.replacement, key_to_node)


def main():
  graph = defaultdict()
  key_to_node = defaultdict()  # since we cannot use nodes as keys
  # to map, use this to map node replacemnt to node.
  inside_marker_lines = False
  changes = set()
  excluded_fields = set()
  for line in sys.stdin:
    line = line.rstrip("\n\r")
    if line == '==== BEGIN EDITS ====':
      inside_marker_lines = True
      continue
    if line == '==== END EDITS ====':
      inside_marker_lines = False
      continue
    if inside_marker_lines:
      changes.add(line)
      continue

    ar = line.split(";")
    # These are fieldDecls
    if len(ar) == 1:
      lhs = GetNode(ar[0])
      # if the field is annotated with RAW_PTR_EXCLUSION,
      # add it to the set of excluded fields
      # this will be later propagated to all neighboring fields.
      if lhs.is_excluded == "1":
        excluded_fields.add(lhs.replacement)
        lhs.is_excluded = "0"
      key_to_node[lhs.replacement] = lhs
      if lhs.replacement not in graph:
        graph.setdefault(lhs.replacement, set())
      continue

    lhs = GetNode(ar[0])
    rhs = GetNode(ar[1])

    # In the case of a typedefNameDecl, all the var/param/fields
    # end up creating the same replacement. What is being done here is
    # that if any field has a typedefNameDecl type, make all matches
    # current and previous marked as is_field
    if lhs.replacement in key_to_node.keys():
      lhs.is_field = "1" if lhs.is_field == "1" or key_to_node[
          lhs.replacement].is_field == "1" else "0"

    if rhs.replacement in key_to_node.keys():
      rhs.is_field = "1" if rhs.is_field == "1" or key_to_node[
          rhs.replacement].is_field == "1" else "0"

    key_to_node[lhs.replacement] = lhs
    key_to_node[rhs.replacement] = rhs

    if lhs.replacement not in graph:
      graph.setdefault(lhs.replacement, set())
    graph[lhs.replacement].add(rhs)

    if rhs.replacement not in graph:
      graph.setdefault(rhs.replacement, set())
    graph[rhs.replacement].add(lhs)

  # Propagate changes to all excluded fields
  visited = set()
  for key in excluded_fields:
    key_to_node[key].is_excluded = "1"
    PropagateExclusions(visited, graph, key, key_to_node)

  visited = set()
  for key in graph.keys():
    node = key_to_node[key]
    if node.is_field == "1" and node.is_excluded == "0" and key not in visited:
      DFS(visited, graph, key, key_to_node, changes)
  changes = sorted(changes)
  for text in changes:
    print(text)
  return 0


if __name__ == '__main__':
  sys.exit(main())
