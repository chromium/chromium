# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation for managing a group of JavaClass nodes."""

import collections
from typing import Set, Tuple

import class_dependency
import graph
import group_json_consts


class JavaGroup(graph.Node):
    """A representation of a group of classes."""

    def __init__(self, group_name: str):
        """Initializes a new Java group structure.

        Args:
            group_name: The name of this group.
        """
        super().__init__(group_name)

        self._classes = {}
        self._class_dependency_edges = collections.defaultdict(set)

    @property
    def classes(self):
        """A map { name -> JavaClass } of classes within this group."""
        return self._classes

    def add_class(self, java_class: class_dependency.JavaClass):
        """Adds a JavaClass to the group, if its key doesn't already exist.

        Notably, this does /not/ automatically update the inbound/outbound
        dependencies of the group with the dependencies of the class.
        """
        if java_class.name not in self._classes:
            self._classes[java_class.name] = java_class

    def add_class_dependency_edge(self, end_group: 'JavaGroup',
                                  begin_class: class_dependency.JavaClass,
                                  end_class: class_dependency.JavaClass):
        """Adds a class dependency edge as part of a group dependency.

        Each group dependency is comprised of one or more class dependencies,
        we manually update the nodes with this info when parsing class graphs.

        Args:
            end_group: the end node of the group dependency edge which starts
                from this node.
            begin_class: the start node of the class dependency edge.
            end_class: the end node of the class dependency edge.
        """
        class_edge = (begin_class, end_class)
        if class_edge not in self._class_dependency_edges[end_group]:
            self._class_dependency_edges[end_group].add(class_edge)

    def get_class_dependencies_in_outbound_edge(self, end_node: 'JavaGroup'
                                                ) -> Set[Tuple]:
        """Breaks down a group dependency edge into class dependencies.

        For group A to depend on another group B, there must exist at least one
        class in A depending on a class in B. This method, given a group
        dependency edge A -> B, returns a set of class dependency edges
        satisfying (class in A) -> (class in B).

        Args:
            end_node: The destination node. This method will return the class
                dependencies forming the edge from the current node to end_node.

        Returns:
            A set of tuples of `JavaClass` nodes, where a tuple (a, b)
            indicates a class dependency a -> b. If there are no relevant
            class dependencies, returns an empty set.
        """
        return self._class_dependency_edges[end_node]

    def get_node_metadata(self):
        """Generates JSON metadata for the current node.

        The list of classes is sorted in order to help with testing.
        Structure:
        {
            'classes': [ class_key, ... ],
        }
        """
        return {
            group_json_consts.CLASSES: sorted(self.classes.keys()),
        }
