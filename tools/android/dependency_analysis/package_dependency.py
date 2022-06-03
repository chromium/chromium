# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of the graph module for a [Java package] dependency graph."""

import collections
from typing import Set, Tuple

import class_dependency
import graph
import package_json_consts


class JavaPackage(graph.Node):
    """A representation of a Java package."""
    def __init__(self, package_name: str):
        """Initializes a new Java package structure.

        Args:
            package_name: The name of the package.
        """
        super().__init__(package_name)

        self._classes = {}
        self._class_dependency_edges = collections.defaultdict(set)

    @property
    def classes(self):
        """A map { name -> JavaClass } of classes within this package."""
        return self._classes

    def add_class(self, java_class: class_dependency.JavaClass):
        """Adds a JavaClass to the package, if its key doesn't already exist.

        Notably, this does /not/ automatically update the inbound/outbound
        dependencies of the package with the dependencies of the class.
        """
        if java_class.name not in self._classes:
            self._classes[java_class.name] = java_class

    def add_class_dependency_edge(self, end_package: 'JavaPackage',
                                  begin_class: class_dependency.JavaClass,
                                  end_class: class_dependency.JavaClass):
        """Adds a class dependency edge as part of a package dependency.

        Each package dependency is comprised of one or more class dependencies,
        we manually update the nodes with this info when parsing class graphs.

        Args:
            end_package: the end node of the package dependency edge
                which starts from this node.
            begin_class: the start node of the class dependency edge.
            end_class: the end node of the class dependency edge.
        """
        class_edge = (begin_class, end_class)
        if class_edge not in self._class_dependency_edges[end_package]:
            self._class_dependency_edges[end_package].add(class_edge)

    def get_class_dependencies_in_outbound_edge(
            self, end_node: 'JavaPackage') -> Set[Tuple]:
        """Breaks down a package dependency edge into class dependencies.

        For package A to depend on another package B, there must exist
        at least one class in A depending on a class in B. This method, given
        a package dependency edge A -> B, returns a set of class
        dependency edges satisfying (class in A) -> (class in B).

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
            package_json_consts.CLASSES: sorted(self.classes.keys()),
        }


class JavaPackageDependencyGraph(graph.Graph):
    """A graph representation of the dependencies between Java packages.

    A directed edge A -> B indicates that A depends on B.
    """
    def __init__(self, class_graph: class_dependency.JavaClassDependencyGraph):
        """Initializes a new package-level dependency graph
        by "collapsing" a class-level dependency graph into its packages.

        Args:
            class_graph: A class-level graph to collapse to a package-level one.
        """
        super().__init__()

        # Create list of all packages using class nodes
        # so we don't miss packages with no dependencies (edges).
        for class_node in class_graph.nodes:
            self.add_node_if_new(class_node.package)

        for begin_class, end_class in class_graph.edges:
            begin_package = begin_class.package
            end_package = end_class.package
            self.add_edge_if_new(begin_package, end_package)

            begin_package_node = self.get_node_by_key(begin_package)
            end_package_node = self.get_node_by_key(end_package)
            begin_package_node.add_class(begin_class)
            end_package_node.add_class(end_class)
            begin_package_node.add_class_dependency_edge(
                end_package_node, begin_class, end_class)

    def create_node_from_key(self, key: str):
        """Create a JavaPackage node from the given key (package name)."""
        return JavaPackage(key)

    def get_edge_metadata(self, begin_node, end_node):
        """Generates JSON metadata for the current edge.

        The list of edges is sorted in order to help with testing.
        Structure:
        {
            'class_edges': [
                [begin_key, end_key], ...
            ],
        }
        """
        return {
            package_json_consts.CLASS_EDGES:
            sorted(
                [begin.name, end.name] for begin, end in
                begin_node.get_class_dependencies_in_outbound_edge(end_node)),
        }
