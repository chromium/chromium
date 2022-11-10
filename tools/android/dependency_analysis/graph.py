# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility classes (and functions, in the future) for graph operations."""

import functools
from typing import Dict, Generic, List, Optional, Tuple, TypeVar


def sorted_nodes_by_name(nodes):
    """Sorts a list of Nodes by their name."""
    return sorted(nodes, key=lambda node: node.name)


def sorted_edges_by_name(edges):
    """Sorts a list of edges (tuples) by their names.

    Prioritizes sorting by the first node in an edge."""
    return sorted(edges, key=lambda edge: (edge[0].name, edge[1].name))

@functools.total_ordering
class Node:
    """A node/vertex in a directed graph."""

    def __init__(self, unique_key: str):
        """Initializes a new node with the given key.

        Args:
            unique_key: A key uniquely identifying the node.
        """
        self._unique_key = unique_key
        self._outbound = set()
        self._inbound = set()

    def __eq__(self, other: 'Node'):
        return self._unique_key == other._unique_key

    def __lt__(self, other: 'Node'):
        return self._unique_key < other._unique_key

    def __hash__(self):
        return hash(self._unique_key)

    def __str__(self) -> str:
        return self.name

    @property
    def name(self):
        """A unique string representation of the node."""
        return self._unique_key

    @property
    def inbound(self):
        """A set of Nodes that have a directed edge into this Node."""
        return self._inbound

    @property
    def outbound(self):
        """A set of Nodes that this Node has a directed edge into."""
        return self._outbound

    def add_outbound(self, node: 'Node'):
        """Creates an edge from the current node to the provided node."""
        self._outbound.add(node)

    def add_inbound(self, node: 'Node'):
        """Creates an edge from the provided node to the current node."""
        self._inbound.add(node)

    def get_node_metadata(self) -> Optional[Dict]:
        """Generates JSON metadata for the current node.

        If the returned dict is None, the metadata field will be excluded."""
        return None


T = TypeVar('T', bound=Node)


class Graph(Generic[T]):
    """A directed graph data structure.

    Maintains an internal Dict[str, T] _key_to_node mapping the unique key of
    nodes to their Node objects. Allows subclasses to specify their own Node
    subclasses via Generic typing.
    """

    def __init__(self):
        self._key_to_node = {}
        self._edges = []

    @property
    def num_nodes(self) -> int:
        """The number of nodes in the graph."""
        return len(self.nodes)

    @property
    def num_edges(self) -> int:
        """The number of edges in the graph."""
        return len(self.edges)

    @property
    def nodes(self) -> List[T]:
        """A list of Nodes in the graph."""
        return list(self._key_to_node.values())

    @property
    def edges(self) -> List[Tuple[T, T]]:
        """A list of tuples (begin, end) representing directed edges."""
        return self._edges

    def get_node_by_key(self, key: str) -> Optional[T]:
        """Returns a node by that key or None if no such node exists."""
        return self._key_to_node.get(key)

    def create_node_from_key(self, key: str) -> T:
        """Given a unique key, creates and returns a Node object.

        Should be overridden by child classes.
        """
        return Node(key)  # type: ignore

    def add_node_if_new(self, key: str) -> T:
        """Adds a Node to the graph.

        A new Node object is constructed from the given key and added.
        If the key already exists in the graph, no change is made to the graph.

        Args:
            key: A unique key to create the new Node from.

        Returns:
            The Node with the given key in the graph.
        """
        try:
            return self._key_to_node[key]
        except KeyError:
            node = self.create_node_from_key(key)
            self._key_to_node[key] = node
            return node

    def add_edge_if_new(self, src: str, dest: str) -> bool:
        """Adds a directed edge to the graph.

        The source and destination nodes are created and added if they
        do not already exist. If the edge already exists in the graph,
        this is a no-op.

        Args:
            src: A unique key representing the source node.
            dest: A unique key representing the destination node.

        Returns:
            True if the edge was added (did not already exist), else False
        """
        src_node = self.add_node_if_new(src)
        dest_node = self.add_node_if_new(dest)

        # The following check is much faster than `if (src, dest) not in _edges`
        if dest_node not in src_node.outbound:
            src_node.add_outbound(dest_node)
            dest_node.add_inbound(src_node)
            self._edges.append((src_node, dest_node))
            return True
        return False

    def get_edge_metadata(self, begin_node, end_node) -> Optional[Dict]:
        """Generates JSON metadata for the current edge.

        If the returned dict is None, the metadata field will be excluded."""
        return None
