#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to extract edits from clang spanification tool output.

The edits have the following format:
    ...
    {lhs_node1};{rhs_node1}
    {node_n}
    {lhs_node2};{rhs_node2}
    ...
    ...
Where lhs_node, rhs_node, and node_n represent a node's text representation
generated using the spanification tool's Node::ToString() function.

The string representation has the following format:
`{is_buffer\,r:::<file path>:::<offset>:::<length>
:::<replacement text>\,include-user-header:::<file path>:::-1:::-1
:::<include text>}\,size_info_available\,is_data_change\,is_deref_node`

where `is_buffer`,`size_info_available`, `is_data_change` and `is_deref_node`
are booleans represented as  0 or 1.

extract_edits.py takes input that is concatenated from multiple tool
invocations and extract just the edits with the following steps:
1- Construct the adjacency list of nodes
   (a pairs of nodes represents an edge in the directed graph)

2- Determine whether size info is available for a given buffer node.

3- Run `DFS` starting from buffer nodes whose size info is available and emit
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

class Node:
    is_buffer = '0'
    replacement = ''
    include_directive = ''
    size_info_available = '0'
    # We need to also rewrite deref expressions of the form:
    # |*buf = something;| into |buf[0] = something;|
    # for that, create a link between buf and the deref expression.
    is_deref_node = '0'
    is_data_change = '0'

    def __init__(self, is_buffer, replacement, include_directive,
                 size_info_available, is_deref_node, is_data_change) -> None:
        self.is_buffer = is_buffer
        self.replacement = replacement
        self.include_directive = include_directive
        self.size_info_available = size_info_available
        self.is_deref_node = is_deref_node
        self.is_data_change = is_data_change

    def __eq__(self, other):
        if isinstance(other, Node):
            return self.replacement == other.replacement
        return False

    def __hash__(self) -> int:
        return hash((self.replacement, self.include_directive))

# Function to parse the string representation of a node and create a Node
# object.
def ParseNode(txt: str):
    # Skipping the first and last character that correspond to the curly braces
    # denoting the start and end of a serialized node.
    x = txt[1:-1].split('\\,')
    # Expect exactly 6 elements that correspond to the following node
    # attributes:
    # - is_buffer
    # - replacement
    # - include_directive
    # - size_info_available
    # - is_deref_node
    # - is_data_change
    assert len(x) == 6
    return Node(*x)


def DFS(visited: set, graph: dict, key: str, key_to_node: dict, changes: set):
    """
    Explore the graph in depth-first search from the given key. Identify edits
    to apply.

    Args:
        visited: A set to track visited nodes. This avoids processing the same
                 node multiple times when there are cycles.
        graph: The graph representing the dependencies between nodes.
        key: The current key being processed.
        key_to_node: A mapping of keys to nodes.
        changes: A set to collect the identified changes.
    """
    # Only visit nodes once:
    if key in visited:
        return
    visited.add(key)

    node = key_to_node[key]
    if not node.replacement.endswith('<empty>'):
        changes.add(node.replacement)
        changes.add(node.include_directive)

    for neighbour in graph[key]:
        DFS(visited, graph, neighbour.replacement, key_to_node, changes)


def SizeInfoAvailable(visited: dict, graph: dict, key: str, key_to_node: dict):
    """
    Determines whether size information is available for a buffer node and its
    neighbors. Updates the node's size_info_available attribute.

    Args:
        visited: Property of Nodes(None, 'visiting', or 'visited').
        graph: The adjacency graph.
        key: The current node's key being processed.
        key_to_node: A key to Node mapping.

    Returns:
        None if a cycle is detected,
        '1' if size information is available for the node and its neighbors,
        '0' otherwise,
    """
    n = key_to_node[key]

    # If we reached a node that contains size info, just return that.
    if n.size_info_available == '1':
        return '1'

    # Cycle detection: If the node is currently being visited, there's a cycle.
    # We can't determine the size info for this node.
    if visited.get(key) == 'visiting':
        return None

    # Memoization: If the node has already been visited, return the result
    # immediately.
    if visited.get(key) == 'visited':
        return n.size_info_available

    visited[key] = 'visiting'
    size_info_available = '0'
    # Check neighbors. If any neighbor doesn't have size info or there's a
    # cycle, the current node also doesn't.
    for neighbour in graph[key]:
        # Break as soon as we encounter a neighbor for which size info is not
        # available.
        if SizeInfoAvailable(visited, graph, neighbour.replacement,
                             key_to_node) == '0':
            size_info_available = '0'
            break
        size_info_available = '1'

    n.size_info_available = size_info_available
    visited[key] = 'visited'
    key_to_node[key] = n
    return size_info_available


def main():
    # Since we cannot use nodes as dict keys, use this to map a key (the
    # replacement directive) to a node.
    key_to_node = dict()
    graph = dict()

    # Collect from every compile units the nodes and edges of the graph:
    for line in sys.stdin:
        line = line.rstrip('\n\r')
        nodes = line.split(';')

        # Parse buffer usage nodes:
        if len(nodes) == 1:
            lhs = ParseNode(nodes[0])
            key_to_node[lhs.replacement] = lhs
            if lhs.replacement not in graph:
                graph[lhs.replacement] = set()
            continue

        # Else, parse the edge between two nodes:
        assert len(nodes) == 2
        lhs = ParseNode(nodes[0])
        rhs = ParseNode(nodes[1])

        # We might have seen this node before determining it's a buffer that
        # needs to be rewritten. Make sure this info is properly stored and not
        # overwritten.
        for node in (lhs, rhs):
            if node.is_buffer == '0' and \
                    node.replacement in key_to_node and \
                    key_to_node[node.replacement].is_buffer == '1':
                node.is_buffer = '1'

            key_to_node[node.replacement] = node
            if node.replacement not in graph:
                graph[node.replacement] = set()

        graph[lhs.replacement].add(rhs)

    # Determine whether size information is available for each buffer node:
    for key in graph:
        node = key_to_node[key]
        visited = dict()

        if node.is_buffer == '1':
            SizeInfoAvailable(visited, graph, key, key_to_node)

    # Collect the changes to apply. We start from buffer nodes whose size
    # info is available and explore the graph in depth-first search.
    changes = set()
    visited = set()
    for key in graph:
        node = key_to_node[key]

        # We only want to rewrite components connected to a buffer node.
        if node.is_buffer != '1':
            continue

        # Some buffers might not have their size info available. We can't
        # rewrite those.
        if node.size_info_available != '1':
            continue

        DFS(visited, graph, key, key_to_node, changes)

    # Iterate over the deref_nodes and then check if their only neighbor was
    # visited. Visited nodes here are nodes who's type was rewritten to span.
    # In that case, the deref expression needs to be adapted(rewritten)
    for key in graph:
        node = key_to_node[key]

        if node.is_deref_node == '1':
            neighbor = list(graph[key])[0]
            if neighbor.replacement in visited:
                changes.add(node.replacement)

    # At the edge in between rewritten and non-rewritten nodes, we need
    # to add a call to `.data()` to access the pointer from the span:
    for key in graph:
        node = key_to_node[key]

        if node.is_data_change != '1':
            continue

        # A data change needs to be added if lhs was not rewritten and rhs was.
        # The lhs key is stored in the data_change_node's include_directive.
        # Check if the lhs key is in the set of visited nodes (i.e lhs was
        # rewritten). If lhs was rewritten, we don't need to add `.data()`
        if node.include_directive in visited:
            continue

        # Expect a single neighbor.
        # TODO(357433195): In practice, this is not always the case. Investigate
        # why and add the assertion back:
        # assert (len(graph[key]) == 1)
        neighbor = list(graph[key])[0]

        # If the rhs node was visited (i.e rewritten), then we need to apply the
        # data change.
        if neighbor.replacement in visited:
            # In this case, rhs was rewritten, and lhs was not, we need to add
            # the corresponding `.data()`
            changes.add(node.replacement)

    for text in changes:
        print(text)
    return 0

if __name__ == '__main__':
    sys.exit(main())
