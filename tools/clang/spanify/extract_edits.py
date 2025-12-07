#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to extract edits from clang spanification tool output.

The edits have the following format:
```
  e lhs_node1 rhs_node2           # Edge from node 1 to node 2.
  e lhs_node2 rhs_node3           # Edge from node 2 to node 3.
  e lhs_node3 rhs_node4           # Edge from node 3 to node 4.
  ...
  s node_1                        # Source node of the graph that triggers a
                                  # rewrite (i.e. buffer usage)
  ...
  i node_4                        # Sink node. A rewrite from a source
                                  # requires the ultimate end nodes to be
                                  # sink. They represent nodes we know can
                                  # be rewrite because the buffer's size
                                  # is known.
  ...
  f lhs_node rhs_node replacement # Span frontier replacement applied if
                                  # lhs_node is not rewritten but rhs_node
                                  # is.
  ...
  r node_1 replacement            # A replacement associated with a node.
```

Where all the `*node*` are abstract ID that represents a node in the graph.

Real example:
```
  s 0008244:DBKYJas7
  s 0008303:GWkNbhQ4
  e 0001450:8-AxbSn3 0008303:GWkNbhQ4
  e 0001518:BUQKDaXe 0008244:DBKYJas7
  e 0001518:L97i_bwg 0008303:GWkNbhQ4
  f 0001450:8-AxbSn3 0008303:GWkNbhQ4 r:::../../base/memory/shared_memory_mapping.h:::8684:::0:::0:::.data()
  f 0001518:BUQKDaXe 0008244:DBKYJas7 r:::../../base/memory/shared_memory_mapping.h:::8535:::15:::0:::(data() + size()).data()
  f 0001518:L97i_bwg 0008303:GWkNbhQ4 r:::../../base/memory/shared_memory_mapping.h:::8686:::15:::0:::(data() + size()).data()
  r 0001518:BUQKDaXe include-user-header:::../../base/containers/checked_iterators.h:::-1:::-1:::base/containers/span.h
  r 0001518:BUQKDaXe r:::../../base/containers/checked_iterators.h:::1518:::9:::0:::base::span<const unsigned char>
  r 0001946:gKWdIpwv r:::../../base/containers/checked_iterators.h:::1946:::9:::0:::base::span<const unsigned char>
```

**Important Note on "r:::" Replacement Directive:**
The `replacement_directive` strings starting with `r:::` (which can appear in
`r` lines or as the third argument in `f` lines) have been extended and are
slightly different from the format accepted by apply_edits.py. There is an
additional `precedence` field before the replacement text.

This `<precedence>` value is used by `extract_edits.py` to merge conflicting
insertions. If multiple `r` directives are insertions (i.e., `<length>` is
"0") and target the exact same file and offset:
    Their `<text>` components are merged into a single replacement.
    Directives with a lower numerical `<precedence>` value have their text
    inserted *earlier* (further to the left) in the final merged text.

The `<precedence>` field is **removed** by `extract_edits.py` from all `r`
directives before they are included in the final list of edits output by this
script.

extract_edits.py takes input that is concatenated from multiple tool
invocations and extract just the edits with the following steps:
1- Construct the adjacency list of nodes
   (a pairs of nodes represents an edge in the directed graph)

2- Determine whether size info is available for a given source node.

3- Run `DFS` starting from source nodes whose size info is available and emit
   edits for reachable nodes.

4- Adapt dereference expressions and add data changes where necessary.

extract_edits.py would then emit the following output:
    <edit1>
    <edit2>
    <edit3>
    ...
Where the edit is either a replacement or an include directive.

For more details about how the tool works, see the doc here:
https://docs.google.com/document/d/1hUPe21CDdbT6_YFHl03KWlcZqhNIPBAfC-5N5DDY2OE/
"""

import sys
import urllib.parse

from os.path import expanduser
import pprint
from collections import defaultdict


# The connected components in the graph. This is useful to split the rewrite
# into atomic changes.
class Component:
    all = set()

    def __init__(self) -> None:
        # Changes associated with the connected component.
        self.changes = set()

        # Frontier changes are either accepted or rejected. The two dictionaries
        # are used to detect conflicts in the frontier changes. This might
        # happen in rare cases where C++ macros are used. In case of conflict,
        # the whole component is discarded.
        self.frontier_changes_accepted = set()
        self.frontier_changes_rejected = set()

        # `Component.all` can be used to iterate over all components.
        Component.all.add(self)

class Node:
    # Mapping in between the node's key and the node.
    key_to_node = dict()

    def __init__(self, key) -> None:
        self.key = key
        self.replacements = set()

        # Neighbors of the node in the graph. The graph is directed,
        # flowing from lhs to rhs.
        self.neighbors_directed = set()
        self.neighbors_undirected = set()

        # Property to track whether the node is "connected" to a source node.
        # This is set from DFS(...)
        self.visited = False

        # The size info is available for a node if all the paths through the
        # graph are leading to a sink. This is initially set to None, and then
        # set by ComputeSizeInfoAvailable(...).
        self.size_info_available = None
        # Track whether this node is currently on the stack of the
        # `ComputeSizeInfoAvailable` recursive function. This is used to detect
        # cycles in the graph.
        self.size_info_visiting = False

        # Identify the connected component this node belongs to. This is set in
        # the main function.
        self.component = None

    def add_replacement(self, replacement: str):
        assert_valid_replacement(replacement)
        self.replacements.add(replacement)

    # Static method to get a node from a replacement key.
    @classmethod
    def from_key(cls: type, key: str):
        # Deduplicate nodes, as they will appear multiple times in the input.
        node = Node.key_to_node.get(key)
        if node is not None:
            return node

        node = Node(key)
        Node.key_to_node[key] = node
        return node

    def __repr__(self) -> str:
        result = [
            f"Node {hash(self)} {{",
            f"  key: {self.key}",
            f"  size_info_available: {self.size_info_available}",
            f"  neighbors_directed: {pprint.pformat([hash(n) for n in self.neighbors_directed], indent=4)}",
            "}",
        ]
        return "\n".join(result)

    # This is not parsable by from_key but is useful for debugging the
    # graph of nodes.
    def to_debug_string(self) -> str:
        return repr(self)

    # Static method to get all nodes.
    @classmethod
    def all(cls: type):
        return cls.key_to_node.values()


def DFS(node: Node):
    """
    Explore the graph in depth-first search from the given node. Identify edits
    to apply.

    Args:
        node: The current node being processed.
    """
    # Only visit nodes once:
    if (node.visited):
        return
    node.visited = True

    for replacement in node.replacements:
        node.component.changes.add(replacement)

    for neighbour in node.neighbors_directed:
        DFS(neighbour)


def ComputeSizeInfoAvailable(node: Node):
    """
    Determines whether size information is available for a source node and its
    neighbors_directed. Updates the node's size_info_available attribute.

    Args:
        node: The current node's being processed.
    """

    # Memoization: node.size_info_available has already been computed. Return.
    if node.size_info_available:
        return

    # If there are no dependencies, the size info is definitely not available
    # for this node.
    if not node.neighbors_directed:
        node.size_info_available = False
        return

    # Cycle: If the node is currently being visited, it means it depends on
    # itself, and there's a cycle. We can't determine the size info for this
    # node with the current implementation.
    if node.size_info_visiting:
        return

    # The size info is available for a node if all the paths through the graph
    # are leading to a sink. Locally, it means all the dependencies have their
    # size info available.
    node.size_info_visiting = True
    for neighbour in node.neighbors_directed:
        ComputeSizeInfoAvailable(neighbour)
    node.size_info_visiting = False

    # This node can be rewritten if all of its dependencies can.
    # Dependencies with `size_info_available == None` are nodes that are part
    # of an isolated cycle. Isolated cycle are rewritten.
    node.size_info_available = not any(
        neighbour.size_info_available == False
        for neighbour in node.neighbors_directed)

# Assert a replacement follows the expected format:
# - r:::<file path>:::<offset>:::<length>:::<replacement text>
# - include-user-header:::<file path>:::-1:::-1:::<include text>
# - include-system-header:::<file path>:::-1:::-1:::<include text>
def assert_valid_replacement(replacement: str):
    try:
        parts = replacement.split(':::')
        directive_type = parts[0]

        assert directive_type in [
            'r', 'include-user-header', 'include-system-header'
        ], f"Unknown directive type '{directive_type}'"

        assert len(parts) > 1
        assert parts[1] != '', "File path must not be empty."

        if directive_type == 'r':
            assert len(
                parts
            ) == 6, f"Directive 'r' must have 6 parts, got {len(parts)}"

            # Validate offset.
            assert parts[2].isdigit()

            # Validate length.
            assert parts[3].isdigit()

            # Validate precedence. Can be a negative or positive integer.
            try:
                int(parts[4])  # Check if it's a valid integer representation
            except ValueError:
                raise AssertionError(
                    f"Precedence '{parts[4]}' must be a valid integer string.")
        else:
            assert len(parts) == 5
    except:
        # Augment the error with the replacement text for better debugging.
        assert False, f"Invalid replacement: \"{replacement}\""


def merge_insertions_and_remove_precedence_field(changes: set) -> set:
    """
    Merges conflicting insertions at the same code location.

    The merge order is determined as follows:
    Handles "associativity" by grouping insertions based on precedence sign.
        The final text is formed by concatenating all positive-precedence
        insertions (i.e., "closing" parts), then zero-precedence, then all
        negative-precedence insertions ("opening" parts). This ensures that
        a closing bracket from a left expression is placed before an opening
        bracket from a right expression (e.g., `...>[...`).
    Also removes the precedence field from the final replacement directives.
    """
    replacements_by_range = defaultdict(list)
    result = set()

    for change in changes:
        assert_valid_replacement(change)
        parts = change.split(':::')
        directive_type = parts[0]

        if directive_type == 'r':
            _, file_path, offset, length, precedence, text = parts
            precedence = int(precedence)
            # Key identifies the exact code range being replaced
            key = (file_path, offset, length)
            replacements_by_range[key].append((precedence, text))
        else:
            result.add(change)

    for key, candidates in replacements_by_range.items():
        assert candidates, "A key should always have at least one candidate."

        file_path, offset, length = key

        if len(candidates) == 1:
            # No conflict.
            _, text = candidates[0]
            reconstructed_directive = f"r:::{file_path}:::{offset}:::{length}:::{text}"
            result.add(reconstructed_directive)
            continue

        if int(length) == 0:
            # Conflicting insertion detected.

            # Assert uniqueness of precedence values to ensure determinism.
            precedences = [p for p, _ in candidates]
            assert len(precedences) == len(
                set(precedences)
            ), "Conflicting insertions need to have unique precedece values."

            merged_texts = [t for _, t in sorted(candidates)]
            reconstructed_directive = f"r:::{file_path}:::{offset}:::{length}:::{''.join(merged_texts)}"
            result.add(reconstructed_directive)
        else:
            # Conflicting non-insertion replacement. This is an unresolvable
            # conflict for now. Just remove the precedence field.
            for _, text in candidates:
                reconstructed_directive = f"r:::{file_path}:::{offset}:::{length}:::{text}"
                result.add(reconstructed_directive)

    return result


def main():
    # Since the tool is invoked from multiple compile units, we are using sets
    # to deduplicate what was visible from multiple compile units.

    # A set of source nodes that trigger the rewrite.
    sources = set()

    # A set of sink nodes. A rewrite from a source requires all the end nodes
    # to be sink. They represent nodes where the rewrite could be applied,
    # because the size info is available.
    sinks = set()

    # Change to apply at the edge in between rewritten and non-rewritten nodes.
    frontiers = set()

    # Collect from every compile units the nodes and edges of the graph:
    for line in sys.stdin:
        line = line.rstrip('\n\r')

        # The first character of the line denotes the type of the line:
        # - 'r': Replacement associated with a node.
        # - 'e': Edge in between two nodes.
        # - 's': Source node of the graph triggering the rewrite.
        # - 'f': Span frontier change.
        # - 'i': Sink node. A rewrite from a source requires the ultimate end
        #        nodes to be sink. They represent nodes we know can be rewrite
        #        because the buffer's size is known.
        assert line[0] in ['r', 'e', 's', 'i', 'f'], "Unknown line type: " +\
               line[0] + " in line: " + line

        # Replacement associated with a node:
        if line[0] == 'r':
            (_, key, replacement) = line.split(' ', 2)
            Node.from_key(key).add_replacement(replacement)
            continue

        # Sink node:
        if line[0] == 'i':
            (_, key) = line.split(' ')
            sinks.add(key)
            continue

        # Source node:
        if line[0] == 's':
            (_, key) = line.split(' ')
            sources.add(key)
            continue

        # Edge in between two nodes:
        if line[0] == 'e':
            (_, lhs_key, rhs_key) = line.split(' ')
            lhs = Node.from_key(lhs_key)
            rhs = Node.from_key(rhs_key)

            # Directed edge:
            lhs.neighbors_directed.add(rhs)

            # Undirected edge:
            lhs.neighbors_undirected.add(rhs)
            rhs.neighbors_undirected.add(lhs)
            continue

        # Span frontier change:
        if line[0] == 'f':
            frontiers.add(line)
            continue

        assert False, "Unreachable code"

    # Mark the sink nodes as rewritable.
    for sink in sinks:
        Node.from_key(sink).size_info_available = True

    # Mark the source nodes:
    source_nodes = []
    for source in sources:
        source_node = Node.from_key(source)
        source_nodes.append(source_node)

        # Determine whether size information is available from this source.
        ComputeSizeInfoAvailable(source_node)

    # Identify all the connected components in the undirected graph. This is
    # exploring the graph in depth-first search and assigning the same component
    # to each node in the connected component.
    for node in Node.all():
        if node.component is not None:
            continue
        new_component = Component()
        stack = [node]
        while stack:
            current = stack.pop()
            if current.component is not None:
                continue
            current.component = new_component
            for neighbor in current.neighbors_undirected:
                stack.append(neighbor)

    # Collect the changes to apply. Starting from sources nodes whose size info
    # could be determined.
    for node in source_nodes:
        # Collect the changes to apply. We start from sources nodes whose size
        # info is available and explore the graph in depth-first search.
        if node.size_info_available:
            DFS(node)

    # At the edge in between rewritten and non-rewritten nodes, we need
    # to add a call to `.data()` to access the pointer from the span:
    for frontier in frontiers:
        (_, lhs_key, rhs_key, replacement) = frontier.split(' ', 3)
        lhs_node = Node.from_key(lhs_key)
        rhs_node = Node.from_key(rhs_key)

        apply_frontier = rhs_node.visited and not lhs_node.visited
        if apply_frontier:
            lhs_node.component.frontier_changes_accepted.add(replacement)
        else:
            lhs_node.component.frontier_changes_rejected.add(replacement)

    # Do or do not, there is no try. Discard components with conflicting
    # frontier changes. This happens in rare cases where C++ macros are used.
    # The whole component is discarded in case of conflict, because we can't
    # satisfy the constraints.
    for component in Component.all:
        if component.frontier_changes_accepted & component.frontier_changes_rejected:
            component.changes.clear()
            continue;

        component.changes |= component.frontier_changes_accepted

    # Emit the changes:
    # - ~/scratch/patches.txt: A summary of each atomic change.
    # - ~/scratch/patch_<patch_index>: Write each atomic change.
    # - stdout: Print a bundle of all the changes. This is usually piped to
    #           "./tools/clang/scripts/apply_edits.py" to apply the changes.

    summary_filename = expanduser('~/scratch/patches.txt')
    summary_file = open(summary_filename, 'w')

    component_with_changes = [
        component for component in Component.all if len(component.changes) > 0
    ]

    for index, component in enumerate(component_with_changes):
        merged_component_changes = merge_insertions_and_remove_precedence_field(
            component.changes)

        for text in merged_component_changes:
            print(text)

        summary_file.write(f'patch_{index}: {len(merged_component_changes)}\n')

        with open(expanduser(f'~/scratch/patch_{index}.txt'), 'w') as f:
            f.write('\n'.join(merged_component_changes))

    summary_file.close()

    return 0


if __name__ == '__main__':
    sys.exit(main())
