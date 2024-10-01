#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to extract edits from clang spanification tool output.

The edits have the following format:
    ...
    {lhs_node1}@{rhs_node1}
    {node_n}
    {lhs_node2}@{rhs_node2}
    ...
    ...
Where lhs_node, rhs_node, and node_n represent a node's text representation
generated using the spanification tool's Node::ToString() function.

The string representation has the following format:
`{is_buffer\,r:::<file path>:::<offset>:::<length>
:::<replacement text>\,include-user-header:::<file path>:::-1:::-1
:::<include text>\,size_info_available\,is_data_change\,is_deref_node}`

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
import urllib.parse

import resource
from os.path import expanduser


# The connected components in the graph. This is useful to split the rewrite
# into atomic changes.
class Component:
    all = set()

    def __init__(self) -> None:
        # Changes associated with the connected component.
        self.changes = set()

        # `Component.all` can be used to iterate over all components.
        Component.all.add(self)


class Node:
    # Mapping in between the replacement directive and the node.
    key_to_node = dict()

    def __init__(self, is_buffer, replacement, include_directive,
                 size_info_available, is_deref_node, is_data_change) -> None:
        self.is_buffer = is_buffer
        self.replacement = replacement
        self.include_directive = include_directive
        # We need to also rewrite deref expressions of the form:
        # |*buf = something;| into |buf[0] = something;|
        # for that, create a link between buf and the deref expression.
        self.is_deref_node = is_deref_node
        self.is_data_change = is_data_change

        # Neighbors of the node in the graph. The graph is directed,
        # flowing from lhs to rhs.
        self.neighbors_directed = set()
        self.neighbors_undirected = set()

        # Property to tracker whether the node is "connected" to a buffer node.
        # This is set from DFS(...)
        self.visited = False

        # See SizeInfoAvailable(...) for more details.
        self.size_info_available = size_info_available
        self.size_info_step = False

        # Identify the connected component this node belongs to. This is set in
        # the main function.
        self.component = None

    def __eq__(self, other):
        if isinstance(other, Node):
            return self.replacement == other.replacement
        return False

    def __hash__(self) -> int:
        return hash((self.replacement, self.include_directive))

    # Static method to get a node from a replacement key.
    def from_key(replacement: str):
        return Node.key_to_node.get(replacement)

    # This is not parsable by from_string but is for debugging.
    def to_debug_string(self) -> str:
        # include_directory already includes explanatory text.
        result = "{"
        result += ("is_buffer:{},replacement:{},{},size_info_available:{}"
                   "is_deref_node:{},is_data_change:{},neighbors:").format(
                       self.is_buffer, self.replacement,
                       self.include_directive, self.size_info_available,
                       self.is_deref_node, self.is_data_change)
        neighbors = "{"
        for node in self.neighbors:
            if len(neighbors) > 1:
                neighbors += ", "
            neighbors += node.to_debug_string()
        neighbors += "}"
        # We started result with a '{' thus we end it to wrap everything up
        # nicely.
        return result + neighbors + "}"


    # Static method to create a node from its string representation. This
    # deduplicate nodes by storing them in a dictionary.
    def from_string(txt: str):
        # Skipping the first and last character that correspond to the curly
        # braces denoting the start and end of a serialized node.
        x = txt[1:-1].split('\\,')
        # Expect exactly 6 elements that correspond to the following node
        # attributes:
        # - is_buffer
        # - replacement
        # - include_directive
        # - size_info_available
        # - is_deref_node
        # - is_data_change
        assert len(x) == 6, txt

        node = Node(*x)

        # Deduplicate nodes, as they might appear multiple times in the input.
        if (Node.key_to_node.get(node.replacement) is None):
            Node.key_to_node[node.replacement] = node

        return Node.key_to_node[node.replacement]

    # This is not parsable by from_string but is useful for debugging the graph
    # of nodes.
    def to_debug_string(self) -> str:
        # include_directory already includes explanatory text so we don't have a
        # string before its value.
        result = "is_buffer:{},replacement:{},{},size_info_available:{}".format(
            self.is_buffer, self.replacement, self.include_directive,
            self.size_info_available)
        result += "is_deref_node:{},is_data_change:{},".format(
            self.is_deref_node, self.is_data_change)
        # Recursively get neighbors_directed.
        result += "neighbors:"
        neighbors_directed = "{"
        for node in self.neighbors_directed:
            if len(neighbors_directed) > 1:
                neighbors_directed += ", "
            neighbors_directed += node.to_debug_string()
        neighbors_directed += "}"
        return result + neighbors_directed

    # Static method to get all nodes.
    def all():
        return Node.key_to_node.values()


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

    if not node.replacement.endswith('<empty>'):
        node.component.changes.add(node.replacement)
        node.component.changes.add(node.include_directive)

    for neighbour in node.neighbors_directed:
        DFS(neighbour)


def SizeInfoAvailable(node: Node):
    """
    Determines whether size information is available for a buffer node and its
    neighbors_directed. Updates the node's size_info_available attribute.

    Args:
        node: The current node's being processed.

    Returns:
        None if a cycle is detected,
        '1' if size information is available for the node and its neighbors,
        '0' otherwise,
    """

    # If we reached a node that contains size info, just return that.
    if node.size_info_available == '1':
        return '1'

    # Cycle detection: If the node is currently being visited, there's a cycle.
    # We can't determine the size info for this node.
    if node.size_info_step == 'visiting':
        return None

    # Memoization: If the node has already been visited, return the result
    # immediately.
    if node.size_info_step == 'visited':
        return node.size_info_available

    node.size_info_step = 'visiting'
    size_info_available = '0'
    # Check neighbors. If any neighbor doesn't have size info or there's a
    # cycle, the current node also doesn't.
    for neighbour in node.neighbors_directed:
        # Break as soon as we encounter a neighbor for which size info is not
        # available.
        if SizeInfoAvailable(neighbour) == '0':
            size_info_available = '0'
            break
        size_info_available = '1'

    node.size_info_available = size_info_available
    node.size_info_step = 'visited'
    return size_info_available


def main():
    # Collect from every compile units the nodes and edges of the graph:
    for line in sys.stdin:
        line = line.rstrip('\n\r')
        nodes = line.split('@')

        # If there's only one node, it's a buffer node.
        if len(nodes) == 1:
            Node.from_string(nodes[0]).is_buffer = '1'
            continue

        # Else, parse the edge between two nodes:
        assert len(nodes) == 2, "Length of nodes: " + str(len(nodes))
        lhs = Node.from_string(nodes[0])
        rhs = Node.from_string(nodes[1])

        # Directed edge:
        lhs.neighbors_directed.add(rhs)

        # Undirected edge:
        lhs.neighbors_undirected.add(rhs)
        rhs.neighbors_undirected.add(lhs)

    # Determine whether size information is available for each buffer node:
    for node in Node.all():
        if node.is_buffer == '1':
            SizeInfoAvailable(node)

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

    # Collect the changes to apply. Starting from buffers nodes whose size info
    # could be determined.
    for node in Node.all():

        # We only want to rewrite components connected to a buffer node.
        if node.is_buffer != '1':
            continue

        # Some buffers might not have their size info available. We can't
        # rewrite those.
        if node.size_info_available != '1':
            continue

        # Collect the changes to apply. We start from buffer nodes whose size
        # info is available and explore the graph in depth-first search.
        DFS(node)

    # Iterate over the deref_nodes and then check if their only neighbor was
    # visited. Visited nodes here are nodes who's type was rewritten to span.
    # In that case, the deref expression needs to be adapted(rewritten)
    for node in Node.all():
        if node.is_deref_node == '1':
            neighbor = list(node.neighbors_directed)[0]
            if neighbor.visited:
                neighbor.component.changes.add(node.replacement)

    # At the edge in between rewritten and non-rewritten nodes, we need
    # to add a call to `.data()` to access the pointer from the span:
    for node in Node.all():

        if node.is_data_change != '1':
            continue

        # A data change needs to be added if lhs was not rewritten and rhs was.
        # The lhs key is stored in the data_change_node's include_directive.
        # Check if the lhs key is in the set of visited nodes (i.e lhs was
        # rewritten). If lhs was rewritten, we don't need to add `.data()`
        if Node.from_key(node.include_directive).visited:
            continue

        # Expect at least single neighbor (usually just one).
        #
        # However when you have a MACRO that references a variable that isn't
        # passed as an argument to that MACRO for example:
        #
        # #define MY_MACRO(name) CallFunc(entry, name)
        #
        # When this macro is used in different locations, each textual
        # replacement causes a single rhs to be created pointing at the location
        # inside the macro (I.E. the |entry| in MY_MACRO). This results in one
        # lhs (the CallFunc's entry parameter) being referenced by two rhs (each
        # macro usage). In that case ALL neighbors should have been visited or
        # NONE should have. I.E. we rewrite all or none. Otherwise a compile
        # error will naturally result.
        num_nodes = len(node.neighbors_directed)
        assert num_nodes >= 1, "and node: " + node.to_debug_string()

        # If the rhs node was visited (i.e rewritten), then we need to apply the
        # data change.
        visited = 0
        neighbors = list(node.neighbors_directed)
        for neighbor in neighbors:
            if neighbor.visited:
                visited += 1
                # In this case, rhs was rewritten, and lhs was not, we need to
                # add the corresponding `.data()`
                neighbor.component.changes.add(node.replacement)
        # It is worth noting that while this is the correct assertion, and there
        # are tests that cover this assertion, in the actual chromium code base
        # we don't ever seem to run into such macros so it is usually num_nodes
        # == 1 and when it isn't visited will be zero. However due to the tests
        # we assert that either nothing gets rewritten or everything gets
        # rewritten.
        assert visited == 0 or visited == num_nodes, ("node: ",
                                                      node.to_debug_string(),
                                                      " num: ", str(num_nodes),
                                                      " visited: ",
                                                      str(visited))

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
        for text in component.changes:
            print(urllib.parse.unquote(text).replace('\n', '\0'))

        summary_file.write(f'patch_{index}: {len(component.changes)}\n')

        with open(expanduser(f'~/scratch/patch_{index}.txt'), 'w') as f:
            f.write('\n'.join(component.changes))

    summary_file.close()

    return 0

if __name__ == '__main__':
    sys.exit(main())
